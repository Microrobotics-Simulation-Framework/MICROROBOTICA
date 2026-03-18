# Design Decisions — MICROBOTICA

> This is a living document. It records the key design decisions made
> during development, their rationale, and their current status.

## D1: Three-Layer USD Composition

**Decision**: Use a session layer as the anonymous root that composes
three sublayers: results (strongest) → override (middle) → base (weakest).

**Rationale**: In USD, sublayers listed first have the strongest opinion.
By placing the results layer at index 0 and the base layer last, simulation
output overrides the original scene values. The base layer is immutable
after load — in a clinical context, this means anatomical geometry from
imaging data is never modified by simulation output.

**Status**: Implemented in `SceneManager::loadScene()`. Verified by
MBCA-VER-001 (base layer immutability) and MBCA-VER-002 (results layer
not persisted).

**Files**: `src/scene/scene_manager.cpp`, `src/scene/results_applicator.cpp`

---

## D2: Blocking receiveResult() with Async Polling

**Decision**: `PhysicsProcess::receiveResult()` is a blocking call with
no timeout. `SimulationController` wraps it in `std::async` and polls via
a `ThreadSafeQueue` from the Qt main thread.

**Rationale**: Keeping the interface blocking is simpler and matches the
natural IPC pattern (ZeroMQ recv). The complexity of non-blocking polling
is contained in `SimulationController`, not spread across every backend
implementation. The downside is documented as MBCA-ANO-004 (indefinite
blocking on backend hang).

**Alternatives considered**:
- Adding `timeout_ms` parameter to `receiveResult()` — rejected because
  every implementation would need its own timeout logic
- Using `std::future::wait_for()` — rejected because it doesn't help if
  the blocking call itself doesn't return

**Status**: Implemented. MBCA-ANO-004 is open, to be resolved in Phase 1
when the ZMQ-based `PhysicsProcess` implementation adds `RCVTIMEO`.

**Files**: `src/core/physics_process.h`, `src/simulation/simulation_controller.cpp`

---

## D3: QT_NO_EMIT for oneTBB Compatibility

**Decision**: Disable Qt's `emit` keyword macro globally via
`add_compile_definitions(QT_NO_EMIT)`. Signal invocations call the
signal method directly (e.g., `frameReady(frame)` instead of
`emit frameReady(frame)`).

**Rationale**: oneTBB's `profiling.h` has an `emit()` method that
conflicts with Qt's `emit` macro. The `emit` keyword is purely syntactic
sugar in Qt — removing it has zero runtime effect. All Qt documentation
and examples work identically without it.

**Status**: Applied globally. All `.cpp` and `.h` files use direct signal
calls.

**Files**: `CMakeLists.txt` (line 36)

---

## D4: Lazy LocalViewport Creation

**Decision**: `ViewportWidget` only creates `LocalViewport` (QOpenGLWidget)
on the first explicit `setRenderMode(LocalHydra)` call, not in the
constructor. The default is `SoftwareViewport`.

**Rationale**: Constructing a `QOpenGLWidget` triggers OpenGL context
creation, which fails in Docker/headless environments and causes "failed
to create drawable" crashes — even if the widget is never shown. Lazy
creation avoids this entirely.

**Status**: Implemented. The application starts with SoftwareViewport
in all environments. Hydra mode can be activated at runtime when a GPU
is available.

**Files**: `src/viewport/viewport_widget.cpp`

---

## D5: ComponentMeta as Plain C++ Struct in src/core/

**Decision**: `ComponentMeta` is a plain C++ struct using only standard
library types and nlohmann_json. No Qt, USD, or Python dependencies.

**Rationale**: ComponentMeta must be attachable to all interfaces
including those in `src/core/` which has zero external dependencies.
It must also be JSON-serializable for CI script consumption
(`harvest_component_meta.py`). Using nlohmann_json (header-only,
already a dependency) keeps it lightweight.

**Trade-off**: Custom `to_json`/`from_json` functions are needed for the
`std::optional<std::string> deprecation_notice` field because
`NLOHMANN_DEFINE_TYPE_INTRUSIVE` doesn't handle `std::optional` even in
nlohmann_json 3.11.

**Status**: Implemented. 12 components have ComponentMeta. The harvest
script validates all IDs are unique and fields are populated.

**Files**: `src/core/component_meta.h`

---

## D6: OpenUSD as Optional Dependency

**Decision**: OpenUSD is detected at configure time via a custom
`FindOpenUSD.cmake` module. When not found, `MICROBOTICA_HAS_USD` is
not defined and all USD code paths are guarded with `#ifdef`.

**Rationale**: OpenUSD is not available via apt on Ubuntu 24.04 and
takes 1–2 hours to build from source. Making it optional means
contributors can build, test, and develop non-USD features immediately.
The Docker base image provides full USD support for CI and anyone who
needs it.

**Status**: Implemented. 28 of 32 tests run without USD. The 4 USD
verification tests emit a Catch2 WARN and skip gracefully.

**Files**: `cmake/FindOpenUSD.cmake`, `CMakeLists.txt`

---

## D7: Docker Base Image with Pre-Built USD

**Decision**: Publish a Docker base image on GHCR containing all
dependencies including OpenUSD 24.08 built from source with oneTBB.

**Rationale**: Building OpenUSD from source is the single biggest barrier
to entry. A pre-built Docker image reduces environment setup from ~2 hours
to ~5 minutes. The same image is used for CI (reproducible builds) and
local development (consistent environment).

**Key build flags**: `--onetbb` (modern TBB, no deprecated warnings),
`--no-materialx --no-embree --no-prman --no-openimageio --no-opencolorio`
(skip heavyweight optional dependencies not needed for MICROBOTICA).

**Status**: Image published at
`ghcr.io/microrobotics-simulation-framework/microrobotica:base`.
CI workflow uses it for both build-test and asan-ubsan jobs.

**Files**: `docker/Dockerfile.base`, `.github/workflows/ci.yml`

---

## D8: Embedded Python via pybind11

**Decision**: The `microrobotica` Python module is embedded in the
application via `PYBIND11_EMBEDDED_MODULE`. The same source file
(`microbota_module.cpp`) can be compiled as a standalone extension
module with `-DBUILD_PYTHON_MODULE=ON`.

**Rationale**: Single-source design prevents API divergence between the
embedded console and the future standalone library. The `ScriptingEngine`
manages stdout/stderr capture and the `Application` owns the
`py::scoped_interpreter` lifetime.

**Known limitation**: Script execution blocks the Qt main thread
(MBCA-ANO-002). Async scripting is deferred to Phase 2.

**Status**: Implemented. `import microrobotica` works in the console.
`sim()` and `scene()` proxies are functional.

**Files**: `src/scripting/microbota_module.cpp`, `src/scripting/scripting_engine.cpp`

---

## D9: Verification Benchmark Registry

**Decision**: Verification tests are registered at static init time via
the `REGISTER_VERIFICATION_BENCHMARK` C++ macro, which populates a global
`std::vector<VerificationBenchmark>`.

**Rationale**: The registry provides a machine-readable manifest of all
verification evidence, linking each test to a component ID, benchmark
type, and description. This supports IEC 62304 SOUP traceability
requirements without manual bookkeeping.

**Macro design**: Uses a 7-parameter form with separate `token` (valid
C++ identifier for static variable naming) and `id_str` (human-readable
benchmark ID string), avoiding C preprocessor token-paste issues with
string literals containing dashes.

**Status**: 6 benchmarks registered (MBCA-VER-001 through MBCA-VER-006).

**Files**: `src/core/verification_registry.h`, `src/core/verification_registry.cpp`

---

## D10: Known Anomaly Registry

**Decision**: Known anomalies are tracked in a structured YAML file
(`docs/validation/known_anomalies.yaml`) with mandatory fields including
safety relevance rationale, detection method, and memory safety relevance.

**Rationale**: IEC 62304 requires SOUP anomaly disclosure. A machine-
readable YAML format enables CI validation (`scripts/check_anomalies.py`)
and ensures every anomaly has the fields a downstream manufacturer needs
for risk assessment.

**Status**: 4 seed anomalies (MBCA-ANO-001 through MBCA-ANO-004). CI
validates schema on every push.

**Files**: `docs/validation/known_anomalies.yaml`, `scripts/check_anomalies.py`
