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
in all environments. Hydra mode is activated at runtime when an OpenGL 4.5
Compatibility context is available (D11).

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

---

## D11: Storm on Mesa at OpenGL 4.5, No NVIDIA Dependency

**Decision**: The viewport asks for OpenGL **4.5** in the Compatibility profile
(not 4.6), and the run scripts pass the host's GPU render nodes
(`--device /dev/dri`, never an NVIDIA GPU's) instead of `--gpus all`. Default
rendering is the Intel GPU through Mesa; Mesa's software rasteriser, llvmpipe,
is the fallback and what CI draws with.

**Rationale**: Hydra Storm (HgiGL, OpenUSD 24.08) needs 4.5 Compatibility.
Mesa provides it on llvmpipe (4.5) and on the Intel GPU (4.6). The old 4.6 gate
admitted only the NVIDIA driver, whose driver deadlocks the maintainer's laptop.
MIME 0.3's discovery measured Storm on llvmpipe, headless, with no GL errors
(`plans/microrobotica/DISCOVERY_MIME03.md` §3, §5c).

**Shared render path**: `StormRenderer` holds the engine, the camera matrices
with clip planes fitted to the stage's bounds, and the lights, with no Qt.
`LocalViewport` draws through it, and so does the headless screenshot test (an
EGL surfaceless context on Mesa), so the test checks the app's own drawing.

**Multisampling**: 4x by default. Storm's alpha-to-coverage draws the
translucent tube and vessel see-through only with a multisample buffer.
`MICROROBOTICA_MSAA=0` trades that for speed on a large software-rendered window.

**Status**: Implemented. Screenshot tests draw the committed demo recording on
llvmpipe and compare it with `tests/fixtures/screenshots/` (in CI), and draw A0's
MIME 0.3 recording when `MICROROBOTICA_A0_RUN` names a run folder
(`scripts/make_a0_fixtures.sh`). Measured on llvmpipe at 640x480, 4x MSAA: A0's
repaints 6-12 ms, the demo's 36-43 ms.

**Files**: `src/viewport/gl_requirements.h`, `src/viewport/storm_renderer.cpp`,
`src/viewport/local_viewport.cpp`, `src/viewport/viewport_widget.cpp`,
`scripts/render-args.sh`, `tests/screenshot/`

---

## D12: MIME 0.3's Live Sessions Through One Isolated Protocol Layer

**Decision**: MICROROBOTICA runs a MIME 0.3 experiment live by spawning
`mime serve <saved folder>` (or connecting to one) and speaking `mime.live/1`.
Every message shape of that protocol lives in `src/live/live_protocol.*` and
nowhere else; the transport (`LiveClient`), the stage mapping (`BindingFrames`)
and the scene pipeline (`LivePhysicsProcess` → `SimulationController` →
`ResultsApplicator`) see only its parsed types.

**Rationale**: MIME's side (its PR #86) was still in its fix round when this
was written: field names, refusals and notice texts could change. One layer,
tested against messages captured from a real `mime serve`
(`tests/fixtures/live/`, `scripts/capture_live_fixtures.py`), keeps such a
change to one file and its test. Parsing is tolerant within the major version,
as the protocol asks: unknown fields are ignored.

**Stage binding, not actor maps**: MIME names the prim each state field moves
(`hello.binding`), every bound prim placed in the world, so a pose is applied
as it stands. MIME's frames are in metres; positions are divided by the
stage's `metersPerUnit`, which the session layer now mirrors from the opened
file (as it mirrors the time codes, D1).

**Readiness from the ready file and hello**, not port polling: `mime serve
--ready-file` names its endpoints once its sockets listen; `hello` describes
the session. The frames socket's handshake is awaited before `hello`, so a
`step` right after connecting keeps its frames.

**Measured** (A0, this laptop, CPU): a session through the app, 910 frames of
900 bytes in about 22 s, none dropped, decode about 35 µs, scene apply about
380 µs a frame.

**Status**: Implemented against MIME PR #86 and its fix round; checked against
MIME's `v0.3` after #86 merged (`21e98f5`), then after its phase 7n-b (PR #89,
the client note's entries 16 to 24) merged (`62d7604`).

**It never goes silent** (MIME's 7n-b): the server answers `hello`, `status`,
`pause` and a stop taken while a chunk runs at once, even while the first
chunk compiles, and acts on `set`, `record`, `pace`, `start` and `step` at
the chunk's end. So each command waits as long as MIME may take to answer it
(`LiveClient::Timeouts`: 5 s at once, 30 s at a chunk's end, 120 s for a stop
that writes its run folder), and a reply to another command than the one
sent is refused. A stop taken while a chunk runs is pending: its reply names
no run folder, and `LivePhysicsProcess::stop` returns once the final frame
comes or the frames' connection closes; a Local session's run folder is then
where its ready file said. The run panel's status line takes the state, the
activity ("compiling") and what is pending from replies, the step and time
from frames, and polls `status` while the first chunk compiles or a pause is
pending, never after a stop.

**The window never waits for MIME.** `LiveClient` sends on worker threads
("lanes"), each with its own REQ socket: the commands lane takes every
command one at a time in the order given (so a pause clicked after a set
reaches MIME after it), and the status lane takes the status polls alone, so
a poll is never queued behind a `set` waiting for the chunk in hand. Outcomes
come back to callbacks, which the main window turns into queued calls on the
UI thread (each tagged with its session, so a late one of an ended session is
dropped). `connect`, start, pause, set, Step, Record and the status polls are
posted; Stop is two phases, the session's `stop` (`requestStop`) and then
`mime serve`'s exit, awaited on its `finished` signal (60 s, then SIGTERM,
then kill 15 s later) rather than with `waitForFinished`. Closing the window
during a session stops it first and closes when it has ended; closing again
waits for MIME no longer. `close()` on the client ends a request in flight
within 100 ms. The synchronous `request()`, `connect()` and `stop()` remain,
as waits on the same lanes, for tests and teardown.

**The security mechanism, from the ready file** (MIME designs #214, #215):
remote sessions will use CurveZMQ over ZMQ end to end. The ready file is
read by mime.live/1's rule: a key the reader does not know is ignored, an
absent `mechanism` means null, and a CURVE session adds `"mechanism":
"curve"` and the server's public key (Z85) there, never in `hello`. For a
Manual endpoint the same two values come from Settings. A client that
cannot speak the named mechanism refuses with a clear message and never
falls back to null: `live::mechanismRefusal` says why, the main window
shows it before connecting, and `LiveClient` refuses every request before
making a socket. This client speaks null alone until MIME's CURVE servers
exist; then each socket (the two command lanes and the frames socket) gets
the server key and the install's own key pair before connecting. The ready
file's key for the server's public key is `server_key` (Z85 text, 40
characters), present only alongside `"mechanism": "curve"`, as MIME's
docs/live.md reserves it.

**The build report** (`mime.report/1`, `src/runs/build_report.*`): `hello`
and a run folder's `report.json` give the validity lines (declared and not
yet checked, computed at the build, or a capability record's limit), the
watches, the flags with their meanings, the settings and the profile. The
claims panel shows them, a declared line never as a verdict; the report's
flags name the frames' flag fields. A run folder without one (written before
7n-b) shows its watches and flags alone; a report of a newer format is not
read, and the panel says so, as MIME's own reader does.

**Server lifetime**: `mime serve` exits when the process that launched it is
gone, so a session MICROROBOTICA launches (Local) never outlives a crashed
viewer. A session started by hand for a Manual endpoint (a remote one, over
SSH) needs `--outlive-launcher` to survive the shell that started it.

**Files**: `src/live/`, `src/runs/build_report.*`, `src/app/main_window.cpp`, `src/panels/live_inputs_panel.cpp`,
`src/panels/claims_panel.cpp`,
`tests/test_live_protocol.cpp`, `tests/test_live_client.cpp`, `tests/test_live_mime_serve.cpp`,
`tests/verification/test_live_stage.cpp`
