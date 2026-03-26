# Changelog

All notable changes to MICROBOTICA will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- **ResultFrame**: `Quatf` struct and `orientations` map for quaternion orientation data (Phase A)
- **ResultFrame**: `MeshData` struct and `meshes` map for per-vertex color data (Phase B)
- **ResultsApplicator**: `xformOp:orient` writing from `ResultFrame::orientations` (MBCA-COMP-011 v1.1.0)
- **ResultsApplicator**: `primvars:displayColor` writing from `ResultFrame::meshes` with vertex count validation
- **StubPhysicsProcess**: Emits rotating quaternion orientation per actor (Z-axis rotation at 1 rad/s)
- **Test fixture**: `test_scene.usda` extended with orient op and FlowField mesh prim

### Verification

- `MBCA-VER-007`: Orientation values map exactly to xformOp:orient (DataIntegrity) — requires USD
- `MBCA-VER-008`: MeshData vertexColors map exactly to primvars:displayColor (DataIntegrity) — requires USD
- `MBCA-SAN-001`: ASan+UBSan clean on commit `e901899` — 41 tests, 1268 assertions, 0 errors (local Docker, CI unavailable — see `docs/validation/sanitizer_runs.yaml`)

- **ConnectionManager** (MBCA-COMP-050): Resolves ZMQ endpoints across Local/Manual/Cloud modes
- **MimePhysicsProcess** (MBCA-IMPL-010): Receives ResultFrames from MIME backend over ZMQ PUB/SUB
- **MimeComputeBackend** (MBCA-IMPL-011): Factory creating MimePhysicsProcess with ConnectionManager endpoint resolution
- **ConnectionConfig**: Mode enum (Local/Manual/Cloud) with endpoint, cluster name, and port configuration
- **`scripts/sky_resolve.py`**: SkyPilot Python API → JSON resolver for Cloud mode (single cluster + `--list`)
- **ZMQ IPC protocol**: REQ/REP (commands) + PUB/SUB (ResultFrame JSON stream) with 10s heartbeat timeout
- **MimeStubPhysicsProcess** (MBCA-IMPL-012): Stub emitting UMR confinement demo data (position, orientation, vertex colors) at ~2 Hz
- **SshTunnel** (MBCA-COMP-051): QProcess-based SSH tunnel manager for Cloud mode with `tunnelDied` signal for preemption detection
- **UMR confinement demo scene**: `umr_confinement.usda` with `/World/Actors/UMR`, `/World/Actors/FieldArrow`, `/World/Environment/Vessel`, `/World/Analysis/FlowField`
- **ExperimentLoader** (MBCA-COMP-060): Loads experiment.json (derived from experiment.yaml), validates schema v1.0, rejects absolute paths, builds PhysicsConfig::actorToPrimPath
- **ExperimentRunner** (MBCA-COMP-061): QProcess-based MIME subprocess launcher with crash detection
- **CloudJobLauncher** (MBCA-COMP-062): SkyPilot job launcher using existing jobs/*.yaml format
- **Connection dialog**: Three-button "No MIME detected" QMessageBox (Launch locally / Enter endpoint / Launch on cloud)
- **`experiment.yaml` schema v1.0**: Physics/control/scene/cloud/sweep/metadata sections; `mime_version_min` at top level; `scripts/yaml_to_json.py` for C++ consumption
- **Example experiment fixture**: `tests/fixtures/example_experiment/` with full directory structure
- **SimulationToolbar**: Play/pause/stop QToolBar driven by SimulationController signals (F5/F6/F7 shortcuts)
- **MimeConsolePanel**: QPlainTextEdit with ANSI color parsing, auto-scroll + scroll lock, source-tagged output
- **AnsiParser**: Stateful ANSI SGR escape code → HTML span converter (bold, underline, 8 fg/bg colors)
- **RunConfigPanel**: Mode selector (Local/Manual/Cloud), experiment dir picker, launch/stop buttons
- **SkyPilotMonitorPanel**: QTableView with sky_resolve.py --list JSON, click-to-connect, 30s auto-refresh
- **ProjectBrowserPanel**: QTreeView + QFileSystemModel rooted at experiment directory
- **SettingsDialog**: 3-tab dialog (General, Theme, Cloud) with QSettings persistence
- **ThemeManager**: Fusion style + Light/Dark/System palette management
- **Layout persistence**: QMainWindow saveState/restoreState with factory reset via View > Reset Layout
- **GraphInspectorPanel**: QGraphicsScene rendering MADDENING node/edge topology from graph.json (read-only), back-edges as dashed arrows, coupling group bounding boxes
- **ScriptEditorPanel**: Tabbed file editor with Python syntax highlighting (PythonHighlighter), QFileSystemWatcher for live external change detection, save/save-all, unsaved changes prompts
- **PythonHighlighter**: QSyntaxHighlighter for Python keywords, builtins, strings, comments, numbers, decorators, multi-line triple-quote strings
- **ParameterPanel**: JSON-driven form (QDoubleSpinBox/QSpinBox/QLineEdit/QCheckBox) for params.py namespace, Apply button emits parametersChanged signal

### Verification

- `MBCA-VER-009`: ResultFrame JSON survives ZMQ PUB/SUB transport without corruption (ProtocolFidelity) — requires ZMQ
- `MBCA-VER-010`: MimeStubPhysicsProcess data arrives at USD prims via demo scene pipeline (DataIntegrity) — requires USD
- `MBCA-SAN-006`: ASan+UBSan clean — 87 tests, 1588 assertions, 0 errors (local Docker, CI unavailable — see `docs/validation/sanitizer_runs.yaml`)
- `MBCA-SAN-005`: ASan+UBSan clean — 87 tests, 1588 assertions, 0 errors (local Docker, CI unavailable — see `docs/validation/sanitizer_runs.yaml`)
- `MBCA-SAN-004`: ASan+UBSan clean — 77 tests, 1568 assertions, 0 errors (local Docker, CI unavailable — see `docs/validation/sanitizer_runs.yaml`)
- `MBCA-SAN-003`: ASan+UBSan clean — 65 tests, 1523 assertions, 0 errors (local Docker, CI unavailable — see `docs/validation/sanitizer_runs.yaml`)
- `MBCA-SAN-002`: ASan+UBSan clean — 60 tests, 1324 assertions, 0 errors (local Docker, CI unavailable — see `docs/validation/sanitizer_runs.yaml`)

## [0.1.0] — 2026-03-17

### Added

- **Build system**: CMake 3.25+ with presets for debug, release, ASan/UBSan, TSan
- **`src/core/` interfaces**: PhysicsProcess (MBCA-COMP-001), RenderBackend (MBCA-COMP-002), RenderSession (MBCA-COMP-003), ComputeBackend (MBCA-COMP-004) — all with ComponentMeta
- **`src/core/` data types**: ResultFrame, PhysicsConfig, RenderConfig, Vec3f, ComponentMeta, SessionProvenance, ThreadSafeQueue
- **`src/core/` infrastructure**: VerificationRegistry with REGISTER_VERIFICATION_BENCHMARK macro, AuditLogger/NullAuditLogger, stability macros (MBCA_EXPERIMENTAL_WARN, MBCA_DEPRECATED)
- **Stub implementations**: StubPhysicsProcess (MBCA-IMPL-001, ~60 Hz sinusoidal data), StubRenderSession, LocalComputeBackend (MBCA-IMPL-002)
- **Scene management**: SceneManager (MBCA-COMP-010) with USD three-layer composition (base/override/results), ResultsApplicator (MBCA-COMP-011), PrimSelection
- **Simulation controller**: SimulationController (MBCA-COMP-020) with std::async worker and ThreadSafeQueue for non-blocking polling
- **Python scripting**: ScriptingEngine, microrobotica embedded module (MBCA-COMP-040) with SceneProxy and SimProxy, pybind11_guard.h for Qt/pybind11 compatibility
- **Viewport**: ViewportWidget (MBCA-COMP-030) with LocalViewport (QOpenGLWidget), SoftwareViewport (fallback), CameraController (orbit/pan/zoom)
- **Panels**: SceneHierarchyPanel, PropertyPanel, TimelinePanel (60 Hz QTimer), ConsoleWidget (MBCA-COMP-041)
- **Application shell**: Application (QApplication + py::scoped_interpreter), MainWindow with dock layout and menu bar
- **Compliance scripts**: check_anomalies.py, check_citations.py, harvest_component_meta.py
- **Documentation**: DOCUMENTATION_ARCHITECTURE.md, intended_use.md, downstream_integration.md, soup_package.md, testing_standards.md, component guide templates, Sphinx/Doxygen configuration
- **Governance files**: README.md (EU MDR disclaimer), CHANGELOG.md, CITATION.cff, CONTRIBUTING.md, SECURITY.md, anomaly issue template

### Verification

- `MBCA-VER-001`: Base layer never written by ResultsApplicator (LayerSeparation) — requires USD
- `MBCA-VER-002`: Results layer never persisted to disk (LayerSeparation) — requires USD
- `MBCA-VER-003`: Position values map exactly to xformOp:translate (DataIntegrity) — requires USD
- `MBCA-VER-004`: Warning logged for unknown prim paths (DataIntegrity) — requires USD
- `MBCA-VER-005`: set_attribute() writes to override layer only (UIBehavior) — PASS
- `MBCA-VER-006`: StubPhysicsProcess 100 frames monotonic, no NaN/Inf (Regression) — PASS

### Known Anomalies

- `MBCA-ANO-001`: ResultsApplicator silently drops unknown prim paths (major, context-dependent)
- `MBCA-ANO-002`: ScriptingEngine blocks Qt main thread (minor, context-dependent)
- `MBCA-ANO-003`: Results layer partially-written on crash (major, context-dependent, memory-safety-relevant)
- `MBCA-ANO-004`: receiveResult() blocks indefinitely on backend hang (major, context-dependent)
