# MICROBOTICA Phase 0 — Implementation Plan

This plan breaks the Phase 0 feature list into implementation batches. Each batch is designed to be completable in a single context window, produces a working commit, and can be resumed by a fresh agent reading only this file plus the referenced documents.

**Reference documents** (an implementing agent should read these):
- `DOCUMENTATION_ARCHITECTURE.md` — the normative architecture document
- `.claude/skills/new-component/SKILL.md` — procedure for adding components
- `.claude/skills/new-verification-test/SKILL.md` — procedure for adding verification tests
- `.claude/skills/new-anomaly/SKILL.md` — procedure for adding anomaly entries
- `.claude/skills/commit-and-push/SKILL.md` — pre-commit checklist

**Current repo state**: LICENSE, README.md (placeholder), DOCUMENTATION_ARCHITECTURE.md, .claude/skills/. Everything else must be created.

**Conventions**:
- `[x]` = complete, `[ ]` = not started, `[~]` = in progress
- Each batch ends with a commit. The commit message prefix is specified.
- Batches 1–4 are strictly sequential. Batches 5–8 have internal ordering but their inter-batch order is flexible.
- "Skill: /name" means follow that skill's procedure. The implementing agent should read and invoke the skill.

---

## Batch 1 — Build system and repository scaffold

**Goal**: A repository that configures, builds (an empty app target and an empty test target), and has all root-level governance files in place. No `src/` code yet.

**Commit prefix**: `feat: repository scaffold and build system`

### Files to create

**Build system:**

- [ ] `CMakeLists.txt` (root) — `cmake_minimum_required(VERSION 3.25)`, `project(MICROBOTICA VERSION 0.1.0 LANGUAGES CXX)`, C++17 standard required. Three targets:
  - `microbotica` — the Qt application executable (empty `main.cpp` for now)
  - `microbotica_tests` — Catch2 test binary (empty test runner for now)
  - `microrobotica` — pybind11 module, gated behind `option(BUILD_PYTHON_MODULE "Build standalone Python module" OFF)`
  - A library target `microbotica_core` for `src/core/` headers — include dirs limited to `src/core/` and vcpkg. No Qt, USD, or Python include dirs. This enforces the dependency-free constraint at build time.
- [ ] `CMakePresets.json` — presets: `linux-debug`, `linux-release`, `linux-asan` (address,undefined), `linux-tsan` (thread). All inherit a `linux-base` hidden preset that sets the generator and vcpkg toolchain file.
- [ ] `vcpkg.json` — dependencies: `nlohmann-json` (>=3.11.0), `spdlog`, `zeromq`, `cppzmq`, `catch2`, `pybind11`. Note: Qt 6.6 and OpenUSD 24.08 are system/manual installs, not vcpkg.
- [ ] `cmake/FindOpenUSD.cmake` — custom find module. Sets `OpenUSD_INCLUDE_DIRS`, `OpenUSD_LIBRARIES` (usd, usdGeom, usdImaging, sdf, tf, gf, vt, ar, plug), `OpenUSD_FOUND`. Searches `PXR_ROOT` env var and common paths.
- [ ] `cmake/CompilerWarnings.cmake` — function `target_enable_warnings(target)` that adds `-Wall -Wextra -Wpedantic -Werror` for GCC/Clang. Applied to MICROBOTICA targets only, not third-party.
- [ ] `src/main.cpp` — minimal: `#include <QApplication>`, creates `QApplication`, returns 0. Placeholder for F0.9.
- [ ] `tests/main.cpp` — Catch2 `#define CATCH_CONFIG_MAIN` entry point (or the Catch2 v3 equivalent: `#include <catch2/catch_session.hpp>` with `Catch::Session().run()`).

**Root-level governance files:**

- [ ] `README.md` — replace current placeholder with the full version from doc arch Section 7: EU MDR-aware disclaimer, documentation table, citation placeholder, AGPL licence reference.
- [ ] `CHANGELOG.md` — Keep a Changelog format with `## [Unreleased]` and sections: Added, Changed, Deprecated, Removed, Fixed, Verification, Security, Known Anomalies.
- [ ] `CITATION.cff` — per doc arch Section 16: cff-version 1.2.0, title MICROBOTICA, type software, version 0.1.0, license AGPL-3.0-or-later, repository-code placeholder, author placeholders.
- [ ] `CONTRIBUTING.md` — development environment setup (Qt 6.6, OpenUSD 24.08, vcpkg), build instructions (`cmake --preset linux-debug && cmake --build build/debug`), commit convention table (from doc arch Section 5), ID prefix convention (`MBCA-`), link to skills documentation.
- [ ] `SECURITY.md` — vulnerability reporting process, response timeline, supported versions.
- [ ] `.github/ISSUE_TEMPLATE/anomaly.md` — structured template with mandatory fields: title, severity dropdown, safety_relevance dropdown, safety_relevance_rationale textarea, affected_components, affected_versions, workaround.

**Sanitizer suppression files:**

- [ ] `lsan.suppressions` — `leak:libpython`, `leak:pybind11`, `leak:Py_Finalize`
- [ ] `tsan.suppressions` — stub with header comment ("Populate with Qt/Python dispatch suppressions when TSan CI is enabled in Phase 1")
- [ ] `valgrind.suppressions` — stub with instructions

### Verification

- [ ] `cmake --preset linux-debug && cmake --build build/debug` succeeds
- [ ] `./build/debug/tests/microbotica_tests` runs (0 tests, 0 failures)
- [ ] `cmake --preset linux-asan && cmake --build build/asan --target microbotica_tests` succeeds

---

## Batch 2 — `src/core/` abstract interfaces and data types

**Goal**: All `src/core/` headers compile against the `microbotica_core` target with zero Qt/USD/Python dependencies. The `ComponentMeta` schema, all abstract interfaces, and all infrastructure headers exist.

**Commit prefix**: `feat: src/core/ abstract interfaces and data types`

### Files to create

**Data types** (all in `src/core/`):

- [ ] `component_meta.h` — `ComponentMeta`, `StabilityLevel`, `ValidatedRegime`, `Reference`; nlohmann_json serialization; `static_assert(NLOHMANN_JSON_VERSION_MAJOR > 3 || (... >= 11), ...)`. Per doc arch Section 8.1.
- [ ] `stability.h` — `MBCA_DEPRECATED(msg)`, `MBCA_EXPERIMENTAL_WARN(class_name)` with `do { static bool _warned = false; ... } while(0)` pattern. Per doc arch Section 8.4.
- [ ] `types.h` — `ProcessStatus` enum (`Idle`, `Launching`, `Running`, `Stopped`, `Error`), `SessionStatus` enum, `RenderMode` enum.
- [ ] `result_frame.h` — `Vec3f` struct (`double x, y, z`), `ResultFrame` struct with `double simTime`, `std::unordered_map<std::string, Vec3f> positions`, `std::unordered_map<std::string, double> scalars`; nlohmann_json serialization.
- [ ] `physics_config.h` — `PhysicsConfig` with `std::unordered_map<std::string, std::string> actorToPrimPath`; nlohmann_json serialization.
- [ ] `render_config.h` — `RenderConfig` struct (render mode, resolution).

**Abstract interfaces** (all in `src/core/`, each with `interfaceMeta()` returning `const ComponentMeta&`):

- [ ] `physics_process.h` — `PhysicsProcess` (MBCA-COMP-001): `launch(PhysicsConfig)`, `receiveResult() → std::optional<ResultFrame>`, `sendParameters(nlohmann::json)`, `stop()`, `status() → ProcessStatus`. Skill: follow `/new-component` Step 1 for interface.
- [ ] `render_backend.h` — `RenderBackend` (MBCA-COMP-002): `initialize()`, `createSession(RenderConfig) → unique_ptr<RenderSession>`, `name() → string`.
- [ ] `render_session.h` — `RenderSession` (MBCA-COMP-003): `requestSession() → string`, `status() → SessionStatus`, `disconnect()`.
- [ ] `compute_backend.h` — `ComputeBackend` (MBCA-COMP-004): `createPhysicsProcess() → unique_ptr<PhysicsProcess>`, `createRenderSession(RenderConfig) → unique_ptr<RenderSession>`, `backendName() → string`.

**Infrastructure:**

- [ ] `verification_registry.h` — `BenchmarkType` enum, `VerificationBenchmark` struct, `benchmarkRegistry()`, `REGISTER_VERIFICATION_BENCHMARK` macro with 7-parameter `token`/`id_str` split. Per doc arch Section 8.3.
- [ ] `audit_logger.h` — `AuditLogger` abstract class with `logEvent()`, `logSessionStart()`, `logParameterChange()`, `logResultFrame()`, `logScriptCommand()`. `NullAuditLogger` concrete class (all no-ops). Per doc arch Section 8.5.
- [ ] `session_provenance.h` — `SessionProvenance` struct with version strings, optional cloud fields, nlohmann_json serialization, `static_assert` for nlohmann >= 3.11. Per doc arch Section 8.2.
- [ ] `verification_registry.cpp` — definition of `benchmarkRegistry()` (returns static local `std::vector`).

### Update CMakeLists.txt

- [ ] Add all `src/core/*.h` and `src/core/*.cpp` to `microbotica_core` target
- [ ] Verify `microbotica_core` compiles with only stdlib + nlohmann_json + spdlog includes

### Verification

- [ ] `cmake --build build/debug --target microbotica_core` succeeds
- [ ] No Qt, USD, or Python headers are included transitively (verified by the limited include dirs on the target)

---

## Batch 3 — Stub implementations and compliance infrastructure

**Goal**: Stub physics and render backends that produce synthetic data. Anomaly registry, compliance scripts, and all `docs/` stubs. After this batch, `scripts/check_anomalies.py` passes.

**Commit prefix**: `feat: stub implementations and compliance infrastructure`

### Stub implementations (`src/stubs/`)

- [ ] `stub_physics_process.h/.cpp` — `StubPhysicsProcess` (MBCA-IMPL-001). `launch()` spawns `std::thread` pushing `ResultFrame` into thread-safe queue at ~60 Hz (sinusoidal `positions["robot"]`, monotonic `simTime`, fixed `scalars["step_out_frequency"]`). `receiveResult()` pops from queue (blocks via condition variable). `stop()` joins thread. `MBCA_EXPERIMENTAL_WARN` in constructor. `meta()` returns `ComponentMeta`.
- [ ] `stub_render_session.h/.cpp` — `StubRenderSession`. `requestSession()` returns `""`. `status()` returns `Disconnected`.
- [ ] `local_compute_backend.h/.cpp` — `LocalComputeBackend`. Creates `StubPhysicsProcess` and `StubRenderSession`. `backendName()` returns `"local"`.

### Compliance infrastructure

**Anomaly registry** — Skill: follow `/new-anomaly` for each entry:

- [ ] `docs/validation/known_anomalies.yaml` — four seed entries (MBCA-ANO-001 through 004) with all schema fields including `resolution_verification`, `detected_by`, `memory_safety_relevant`. MBCA-ANO-001 ships as `resolution_status: "fixed"`, `resolution_verification: "MBCA-VER-004"`.

**Regulatory stubs:**

- [ ] `docs/regulatory/intended_use.md` — full content: platform positioning statement, layered responsibility table, cybersecurity boundary statement (including "MICROBOTICA does not implement security controls" sentence), AGPL licence statement. Per doc arch Section 2.
- [ ] `docs/regulatory/downstream_integration.md` — skeleton: four-layer chain, commercial boundary statement, forward reference to Phase 3.
- [ ] `docs/validation/soup_package.md` — skeleton Sections 1–3.

**CI validation scripts:**

- [ ] `scripts/check_anomalies.py` — validates YAML schema; `--prefix MBCA-ANO-`; checks fixed anomalies have `resolution_verification`; checks `memory_safety_relevant: true` anomalies have explicit `safety_relevance`. Executable via `python scripts/check_anomalies.py`.
- [ ] `scripts/check_citations.py` — validates `[@Key]` citations against `docs/bibliography.bib`. Passes on empty bib.
- [ ] `scripts/harvest_component_meta.py` — parses `src/` headers for `ComponentMeta` initialisations; validates required fields; checks no duplicate IDs; outputs JSON to stdout.

### Documentation stubs

- [ ] `docs/bibliography.bib` — empty with header comment explaining `[@Key]` convention
- [ ] `docs/user_guide/index.md` — placeholder
- [ ] `docs/user_guide/concepts.md` — placeholder
- [ ] `docs/developer_guide/index.md` — placeholder
- [ ] `docs/developer_guide/component_authoring.md` — new component checklist (from skill)
- [ ] `docs/developer_guide/testing_standards.md` — three test tiers + full memory safety section (ASan/UBSan/TSan/Valgrind, CMake presets, suppression files, CI structure, regulatory note)
- [ ] `docs/component_guide/index.md` — placeholder
- [ ] `docs/component_guide/interfaces/_template.md` — from doc arch Section 3
- [ ] `docs/component_guide/implementations/_template.md`
- [ ] `docs/validation/index.md` — V&V philosophy, faithful rendering boundary, verification domain table
- [ ] `docs/regulatory/eu_mdr_guidelines.md` — stub ("Phase 3 — content pending")
- [ ] `docs/regulatory/iec62304_mapping.md` — stub ("Phase 1 — content pending")
- [ ] `docs/regulatory/mdcg_2019_11.md` — stub ("Phase 3 — content pending")
- [ ] `docs/regulatory/usability_engineering.md` — stub ("Phase 1 — content pending")

### Doxygen and Sphinx

- [ ] `Doxyfile` — `GENERATE_XML=YES`, `GENERATE_HTML=NO`, `INPUT=src/`, `RECURSIVE=YES`, output to `docs/_doxygen/`
- [ ] `docs/conf.py` — Sphinx config with myst-parser, breathe, sphinxcontrib-bibtex. Comment block for future multiproject mounts.
- [ ] `docs/index.rst` — documentation root linking to all sections

### Update CMakeLists.txt

- [ ] Add `src/stubs/*.cpp` to `microbotica` target sources

### Verification

- [ ] `python scripts/check_anomalies.py` exits 0
- [ ] `python scripts/check_citations.py` exits 0
- [ ] `python scripts/harvest_component_meta.py` exits 0 and prints JSON with all ComponentMeta IDs
- [ ] `doxygen Doxyfile` succeeds
- [ ] `sphinx-build docs/ docs/_build/html` succeeds (warnings OK, errors not OK)

---

## Batch 4 — Scene management and ResultsApplicator

**Goal**: USD three-layer composition working. `SceneManager` loads a USD file, creates override and results sublayers, and `ResultsApplicator` writes `ResultFrame` data to the results layer. Layer separation verification tests pass.

**Commit prefix**: `feat: USD scene management and ResultsApplicator`

### Scene management (`src/scene/`)

- [ ] `scene_manager.h/.cpp` — `SceneManager` (MBCA-COMP-010): loads base layer, creates in-memory override + results sublayers, edit target enforcement (debug asserts), `applyResultFrame()` delegates to `ResultsApplicator`, `crashRecovery()` clears results layer, `saveOverrideLayer()` persists override only. Qt signals: `sceneLoaded()`, `stageChanged()`, `primChanged(SdfPath)`, `backendCrashed()`. Accepts `AuditLogger&` defaulting to `NullAuditLogger`.
- [ ] `layer_stack.h/.cpp` — `LayerStack`: tracks base/override/results `SdfLayerHandle` trio; `isBaseLayer()`, `isResultsLayer()`, `sublayerPaths()`.
- [ ] `prim_selection.h/.cpp` — `PrimSelection`: holds selected `SdfPath`; signal `primSelected(SdfPath)`.
- [ ] `results_applicator.h/.cpp` — `ResultsApplicator` (MBCA-COMP-011): maps `ResultFrame::positions` to `xformOp:translate` via `actorToPrimPath`; **logs `spdlog::warn` for unknown prim paths** (fixes MBCA-ANO-001); maps scalars to custom attributes.

### Test USD scene file

- [ ] `tests/fixtures/test_scene.usda` — minimal USD scene with `/World` root and `/World/Robot` xformable prim. Used by all scene/layer tests.

### Unit tests

- [ ] `tests/test_result_frame.cpp` — construct, populate, copy, map access
- [ ] `tests/test_physics_config.cpp` — actorToPrimPath roundtrip via JSON
- [ ] `tests/test_stub_physics_process.cpp` — launch + 10x receiveResult; assert simTime monotonic, positions populated, scalars present, status transitions

### Verification tests — Skill: follow `/new-verification-test` for each

- [ ] `tests/verification/test_layer_stack.cpp`:
  - MBCA-VER-001 (MBCA-COMP-010, LayerSeparation) — base layer never written by ResultsApplicator
  - MBCA-VER-002 (MBCA-COMP-010, LayerSeparation) — results layer not persisted to disk
- [ ] `tests/verification/test_results_applicator.cpp`:
  - MBCA-VER-003 (MBCA-COMP-011, DataIntegrity) — position values map exactly to xformOp:translate
  - MBCA-VER-004 (MBCA-COMP-011, DataIntegrity) — warning logged for unknown prim paths (should PASS — fix is implemented)
- [ ] `tests/verification/test_stub_regression.cpp`:
  - MBCA-VER-006 (MBCA-COMP-001, Regression) — 100 frames, simTime monotonic, no NaN/Inf

### Update CMakeLists.txt

- [ ] Add `src/scene/*.cpp` to `microbotica` target
- [ ] Add test sources to `microbotica_tests`
- [ ] Link `microbotica_tests` against OpenUSD libraries (usd, usdGeom, sdf, tf, gf, vt)

### Verification

- [ ] All unit tests pass
- [ ] All verification tests pass (including MBCA-VER-004)
- [ ] `cmake --preset linux-asan && cmake --build build/asan --target microbotica_tests && LSAN_OPTIONS=suppressions=lsan.suppressions ./build/asan/tests/microbotica_tests` — no ASan errors
- [ ] Skill: run `/commit-and-push` checklist

---

## Batch 5 — SimulationController and integration wiring

**Goal**: The simulation loop works end-to-end with synthetic data: `LocalComputeBackend → StubPhysicsProcess → ResultFrame → SceneManager → USD`. The `std::async` non-blocking polling architecture is in place.

**Commit prefix**: `feat: SimulationController and end-to-end simulation loop`

### Simulation controller (`src/simulation/`)

- [ ] `simulation_controller.h/.cpp` — `SimulationController` (MBCA-COMP-020): owns `PhysicsProcess` via `unique_ptr`; `setComputeBackend()`, `launchPhysics(PhysicsConfig)`, `stop()`, `teardown()`. Worker: `std::async` calls blocking `receiveResult()` in loop, pushes to thread-safe queue. `requestNextFrame()` polls queue non-blockingly; if frame ready, emits `frameReady(ResultFrame)` and calls `sceneManager.applyResultFrame()`. Crash detection: if async worker catches exception or receives `nullopt`, sets atomic error flag; next `requestNextFrame()` detects and calls `sceneManager.crashRecovery()`, emits `backendCrashed(QString)`. Accepts `AuditLogger&`.

### Thread-safe queue utility

- [ ] `src/core/threadsafe_queue.h` — `ThreadSafeQueue<T>`: `push(T)`, `try_pop() → std::optional<T>` (non-blocking), `wait_pop() → T` (blocking). Uses `std::mutex` + `std::condition_variable`. Used by SimulationController's async worker.

### Tests

- [ ] `tests/test_stub_render_session.cpp` — requestSession returns empty, status Disconnected
- [ ] `tests/test_integration_wiring.cpp` — `LocalComputeBackend → SimulationController → 10 frames → SceneManager::applyResultFrame()` → assert `/World/Robot` translate != (0,0,0) in results layer; assert base layer unchanged

### Verification

- [ ] Integration test passes
- [ ] ASan build passes on all tests
- [ ] Skill: run `/commit-and-push` checklist

---

## Batch 6 — Python scripting engine and MicrobotaModule

**Goal**: The embedded Python interpreter works. `import microrobotica` succeeds. The `microrobotica.scene()` and `microrobotica.sim()` proxies read and write through the correct layers. MBCA-VER-005 passes.

**Commit prefix**: `feat: Python scripting engine and microrobotica module`

### Scripting engine (`src/scripting/`)

- [ ] `pybind11_guard.h` — `#pragma push_macro("slots")` / `#undef slots` / `#pragma pop_macro("slots")` around pybind11 includes
- [ ] `scripting_engine.h/.cpp` — `ScriptingEngine` singleton. Does NOT own `py::scoped_interpreter` (that lives in `Application`). `initialize()` sets `sys.path`, imports `microrobotica`. `execute(code, stdoutOut, stderrOut) → bool`.
- [ ] `MicrobotaModule.cpp` (MBCA-COMP-040) — `PYBIND11_EMBEDDED_MODULE("microrobotica", m)`. Exposes `SceneProxy` (`prim_paths()`, `get_attribute()`, `set_attribute()`) and `SimProxy` (`current_time()`, `current_frame()`, `send_parameters()`, `status()`). `set_attribute()` raises `RuntimeError` when sim is Running. All calls dispatch to Qt main thread via `QMetaObject::invokeMethod`.

### Build note

The standalone `microrobotica` Python extension (`BUILD_PYTHON_MODULE=ON`) uses the same `MicrobotaModule.cpp`. Add the conditional CMake target:

```cmake
if(BUILD_PYTHON_MODULE)
    pybind11_add_module(microrobotica src/scripting/MicrobotaModule.cpp)
    target_include_directories(microrobotica PRIVATE src)
endif()
```

### Tests

- [ ] `tests/test_scripting_engine.cpp` — initialize interpreter; `import microrobotica` succeeds; `microrobotica.sim().current_time()` returns 0.0; `set_attribute()` during Running raises RuntimeError; `send_parameters()` with non-string key raises TypeError

### Verification tests

- [ ] `tests/verification/test_scripting_api.cpp`:
  - MBCA-VER-005 (MBCA-COMP-040, UIBehavior) — `set_attribute()` writes to override layer only; base and results layers unchanged

### Verification

- [ ] All tests pass (unit + verification + integration)
- [ ] ASan build passes
- [ ] `python scripts/harvest_component_meta.py` includes MBCA-COMP-040
- [ ] Skill: run `/commit-and-push` checklist

---

## Batch 7 — Viewport and camera

**Goal**: A `QOpenGLWidget` viewport renders a USD stage via Hydra/Storm. If the Hydra FBO integration is intractable, the `SoftwareViewport` fallback is used instead. Camera orbit/pan/zoom works.

**Commit prefix**: `feat: viewport rendering with Hydra/Storm`

### Viewport (`src/viewport/`)

- [ ] `viewport_widget.h/.cpp` — `ViewportWidget` (MBCA-COMP-030): `QStackedWidget` switching `LocalViewport` (index 0) and `StreamViewport` (index 1). `setStage()`, `setCamera()`, `setRenderMode()`.
- [ ] `local_viewport.h/.cpp` — `LocalViewport`: `QOpenGLWidget`; `UsdImagingGLEngine` in `initializeGL()`; `paintGL()` with Hydra FBO blit.
- [ ] `stream_viewport.h/.cpp` — `StreamViewport`: `QWebEngineView`; `setStreamUrl()`; placeholder message when empty.
- [ ] `camera_controller.h/.cpp` — `CameraController`: orbit (left drag), pan (middle drag), zoom (scroll); manages `GfCamera`; emits `cameraChanged(GfCamera)`.

**Escape hatch** — if Hydra FBO proves intractable:
- [ ] `software_viewport.h/.cpp` — `SoftwareViewport`: `QLabel` + offscreen `UsdImagingGLEngine::Render()` → `QPixmap`. Named class, not a hack. `ViewportWidget` switches to this instead of `LocalViewport`.

### Application shell update

- [ ] Update `src/main.cpp` — set `QSurfaceFormat` (OpenGL 4.5 Core Profile) before `QApplication` construction.

### Verification

- [ ] Application launches and shows a viewport (either Hydra or software fallback)
- [ ] Loading `tests/fixtures/test_scene.usda` renders `/World/Robot` visible
- [ ] Camera orbit/pan/zoom responds to mouse input
- [ ] If fallback was used: document in CHANGELOG and note in code which issue blocked Hydra

---

## Batch 8 — Panels, application shell, and final integration

**Goal**: Full application shell with all in-scope panels wired up. The user can open a USD file, see it in the viewport, browse the hierarchy, edit properties, run the stub simulation, and use the Python console. This is Phase 0 complete.

**Commit prefix**: `feat: panels, application shell, and Phase 0 integration`

### Panels (`src/panels/`)

- [ ] `scene_hierarchy_panel.h/.cpp` — `SceneHierarchyPanel`: `QTreeView` + `UsdPrimModel` (`QAbstractItemModel`); lazy loading; columns Name/Type/Active; fires `primSelected(SdfPath)`.
- [ ] `usd_prim_model.h/.cpp` — `UsdPrimModel`: `QAbstractItemModel` wrapping USD prim hierarchy.
- [ ] `property_panel.h/.cpp` — `PropertyPanel`: subscribes to `primSelected`; lists attributes; edits write to override layer; "overridden" badge.
- [ ] `timeline_panel.h/.cpp` — `TimelinePanel`: play/pause/stop + scrub; `QTimer` 60 Hz; drives `SimulationController::requestNextFrame()`.
- [ ] `console_widget.h/.cpp` — `ConsoleWidget` (MBCA-COMP-041): `QPlainTextEdit` output + `QLineEdit` input; history; submits to `ScriptingEngine::execute()`.

### Application shell (`src/app/`)

- [ ] `application.h/.cpp` — `Application`: `QApplication` subclass; owns `py::scoped_interpreter`; sets `QSurfaceFormat` before construction.
- [ ] `main_window.h/.cpp` — `MainWindow`: `QMainWindow` with dock layout (left: hierarchy+property, bottom: timeline+console, central: viewport). Menu bar: File (Open, Close, Quit), View (toggle docks), Simulation (Start, Stop, Compute Backend submenu with Local), Scripting (Script Editor, Clear Console), Help (About). Status bar: simTime, processStatus, renderMode. `QSettings` save/restore of window state.
- [ ] Update `src/main.cpp` — create `Application`, create `MainWindow`, show, exec.

### Deferrable panel (implement if time permits)

- [ ] `metrics_panel.h/.cpp` — `MetricsPanel`: `QChartView` + `QLineSeries`; subscribes to `frameReady(ResultFrame)`. Requires `Qt6::Charts`.

### Final wiring

- [ ] Wire `MainWindow` → `SimulationController` → `SceneManager` → `ViewportWidget`
- [ ] Wire `SceneHierarchyPanel` → `PrimSelection` → `PropertyPanel`
- [ ] Wire `TimelinePanel` → `SimulationController::requestNextFrame()`
- [ ] Wire `ConsoleWidget` → `ScriptingEngine::execute()`
- [ ] Wire `Simulation → Start` menu action → `SimulationController::launchPhysics()`
- [ ] Wire `File → Open` → `SceneManager::loadScene(path)`

### Verification

- [ ] Application launches, opens a USD file, shows hierarchy and viewport
- [ ] Starting simulation produces synthetic robot movement in the viewport
- [ ] Python console: `import microrobotica; microrobotica.sim().status()` returns `"idle"`
- [ ] Stopping simulation stops the robot
- [ ] Window state persists across restarts
- [ ] All unit + verification + integration tests still pass
- [ ] ASan build still passes
- [ ] All compliance scripts pass
- [ ] Skill: run `/commit-and-push` checklist (full pass)

---

## Batch 9 — Final compliance pass and release prep

**Goal**: All documentation is consistent with the implemented code. CHANGELOG is populated. All Phase 0 checklist items from doc arch Appendix D are verified complete.

**Commit prefix**: `docs: Phase 0 compliance pass and release preparation`

### Documentation updates

- [ ] Update `CHANGELOG.md` `## [0.1.0]` with all Phase 0 additions
- [ ] Update `docs/validation/known_anomalies.yaml` — verify MBCA-ANO-001 is `"fixed"` and MBCA-ANO-002/003/004 are accurate
- [ ] Update `docs/validation/soup_package.md` Sections 1–3 with actual test counts, component list, anomaly summary
- [ ] Regenerate `docs/_doxygen/` and rebuild Sphinx site — verify clean build
- [ ] Run `scripts/harvest_component_meta.py` — verify all ComponentMeta IDs are present and correct

### Appendix D Phase 0 checklist verification

Walk through every item in `DOCUMENTATION_ARCHITECTURE.md` Appendix D Phase 0 checklist and confirm:

- [ ] All repository root files exist and have correct content
- [ ] All docs/ directories and stubs exist
- [ ] `intended_use.md` has all required sections
- [ ] `downstream_integration.md` skeleton has four-layer chain
- [ ] `known_anomalies.yaml` validates
- [ ] `soup_package.md` skeleton has Sections 1-3
- [ ] `ComponentMeta` on all `src/core/` interfaces
- [ ] GitHub labels created (manual step — note for user)
- [ ] CI scripts all pass
- [ ] `CMakePresets.json` has `linux-asan` preset
- [ ] `lsan.suppressions` exists

### Skill: run full `/commit-and-push` checklist

- [ ] Build + tests pass
- [ ] ASan passes
- [ ] All three compliance scripts pass
- [ ] CHANGELOG updated
- [ ] Commit and push

---

## Context window restart guide

If a context window runs out mid-batch, the resuming agent should:

1. Read this file (`IMPLEMENTATION_PLAN_PHASE0.md`) to find where we are
2. Read `DOCUMENTATION_ARCHITECTURE.md` for architectural decisions
3. Read the relevant skill file(s) for the current batch
4. Check the checklist state in this file — items marked `[x]` are done
5. Run `cmake --build build/debug --target microbotica_tests && cd build/debug && ctest --output-on-failure` to verify current state
6. Continue from the first unchecked `[ ]` item

**Important**: after each batch is complete, update this file to mark items `[x]` before committing. This is the persistent progress tracker.

---

## Dependency graph

```
Batch 1 (scaffold)
    ↓
Batch 2 (src/core/)
    ↓
Batch 3 (stubs + compliance + docs)
    ↓
Batch 4 (scene + ResultsApplicator + verification tests)
    ↓
Batch 5 (SimulationController + integration)
    ↓
Batch 6 (scripting engine + Python module)
    ↓
Batch 7 (viewport + camera)
    ↓
Batch 8 (panels + app shell + final wiring)
    ↓
Batch 9 (compliance pass + release prep)
```

Batches 6 and 7 are independent of each other (both depend on Batch 5) and could theoretically be swapped. All other ordering is strict.

---

## What is NOT in this plan

- Cloud backends (SkyPilot, Selkies) — Phase 2
- Real MIME/MADDENING integration — Phase 1
- `.microbotica` project file format — Phase 1
- Full ComputePanel — Phase 1
- TSan/Valgrind CI jobs — Phase 1 (presets and suppression stubs exist)
- Full component guide documents — Phase 1
- Full regulatory documents — Phase 1/3
- Sphinx multiproject mounts — Phase 2+
- MetricsPanel — deferrable within Phase 0 (Batch 8), not blocking
