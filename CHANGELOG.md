# Changelog

All notable changes to MICROBOTICA will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- **Base image**: the NVIDIA container variables are gone (nothing reads them: the viewport draws on Mesa), and Qt
  Charts (`qt6-charts-dev`, Qt 6.4) is added for the plots panel, in a layer after OpenUSD's. Built images are tagged
  `base-<commit>`; only main moves the shared `base` tag. CI builds in the image `docker/base-image` names, so a branch
  that changes the image tests its own build.
- **Viewport: no NVIDIA dependency.** Hydra Storm needs OpenGL 4.5 Compatibility, not 4.6: the GL gate is lowered,
  so the viewport draws on Mesa, on the Intel GPU (`--device /dev/dri`) or its software rasteriser (llvmpipe). The
  run scripts pass the host's render nodes (never an NVIDIA GPU's) instead of `--gpus all` (`scripts/render-args.sh`;
  `MICROROBOTICA_RENDER=llvmpipe` forces software). `MICROROBOTICA_MSAA` overrides the 4x multisampling.

### Fixed

- **A stage's units and up axis survive the session layer**: `metersPerUnit` and `upAxis` are mirrored from the opened
  file onto the anonymous session layer, as the time codes are; the composed stage read USD's fallbacks (centimetres,
  Y up) before.

### Removed

- **MIME 0.2's ZMQ adapter** (`MimePhysicsProcess`, `MimeComputeBackend`), its binary `ResultFrame` decoder with the
  `stream_info` schema handshake, and zstd frame compression: `src/live/` decodes mime.live/1's binary frames (a header
  and the schema `hello` gives) and its JSON frames, kept for debugging.
- **The parameter panel** and its `{"command": "params"}`: a MIME 0.3 session changes only its controller's declared
  live inputs (`set`); a physical setting is a rebuild.

- **MIME 0.2's experiment form**: the in-repository `experiments/` (A0 as a MIME 0.3 saved folder is the reference
  experiment now), `ExperimentLoader` (MBCA-COMP-060) and `experiment.yaml`/`experiment.json`, the `physics/params.py`
  runpy that filled the parameter panel, `scripts/yaml_to_json.py`, and the spawn of the 0.2 runner
  (`python -m mime.runner`) with its `stream_info` readiness polling. **File → Open Saved Experiment** opens a MIME 0.3
  saved folder (`experiment.mime.yaml`, `scene/scene.usda`); running it live comes with the `mime.live/1` client.
- **Bare-string commands**: `launch` and `stop` were sent as bare strings, never JSON, so the 0.2 runner refused them
  and a Manual-mode session never stopped. Every command is a JSON object now (`{"command": "start"}`,
  `{"command": "stop"}`); a reply that is not a JSON object accepting it is a refusal.
- **Cloud (SkyPilot) mode, set aside**: taken out of the Run Configuration panel and the main window. MIME 0.3's remote
  answer is `mime serve` behind an SSH tunnel (Manual endpoint); pixel streaming may return with MADDENING 0.5.0.
  `CloudJobLauncher`, `SshTunnel`, the SkyPilot monitor panel and their tests stay in the tree, unwired.

### Added

- **Run folders as the main entry** (File → Open Run Folder, or a run folder on the command line): a MIME 0.3 run
  folder opens with its recording playing (or its experiment's stage), its experiment (Launch runs it again, live), its
  topology in the graph inspector, its recorded series in the plots panel and MIME's claims about it. A
  `recording.usdc` opened from File → Open USD Scene opens as its run folder's. MIME's visuals overlay is added when
  `mime visuals view` names one (the AR4's meshes, when the user fetched them; simple shapes otherwise).
- **Plots panel**: observation outputs and flags over time in their units, live (every scalar a session's frames
  carry) or from a run folder's `outputs.npz` (what the run recorded per step); flags shaded where raised; a cursor
  following playback. Drawn with Qt Charts when the build has it. `src/runs/` reads `outputs.npz` (stored or deflated
  members) and the run's manifest (RunFolder, MBCA-COMP-070; MBCA-VER-017).
- **Claims panel**: what MIME says of a run, as it says it: the validity lines it was watched against (A0: its Reynolds
  number is above Stokes flow's bound), each flag with its meaning and whether it was raised, and the profile with what
  it has pending; during a live session, each flag at the time it was first raised.
- **Replay this session** (Simulation menu): a live session's run folder replayed from its command log
  (`mime run <experiment> --commands commands.jsonl`) into a new run folder, which then opens.
- **Playback speed**: 0.1x to 4x on the timeline.
- **Live sessions of MIME 0.3 experiments** (`mime.live/1`, MIME's `mime serve`): Launch on an opened saved experiment
  spawns `mime serve` (Settings → MIME command, or `MICROROBOTICA_MIME`) and connects once its ready file appears;
  Manual endpoint connects to a session already serving (a remote one through an SSH tunnel). `hello` gives the stage
  (opened, with MIME's visuals overlay when the asset cache has one; what is missing said otherwise), the stage
  binding (each frame's poses on their prims, in the stage's units; field vectors as arrows), the topology (the graph
  inspector) and the controller's live inputs. Start, pause, stop, Step (F8) and Record are the protocol's commands;
  Stop names the run folder the session wrote. `src/live/` (MBCA-IMPL-013); tests against messages captured from a
  real `mime serve` (MBCA-VER-014, -015) and a live frame on a millimetre stage (MBCA-VER-016); an opt-in test against
  a running `mime serve` (`MICROROBOTICA_LIVE_READY`).
- **Live-inputs panel** (replaces the parameter panel): the controller's declared live inputs, editable and sent with
  `set`; the experiment's settings read-only from its lock; "Rebuild from the saved folder".
- **Graph inspector on MIME 0.3's topology**: nodes coloured by their entry's kind, tooltips with entries, settings,
  units and when each edge reads.
- **Command line**: `microbotica [folder or USD file] [--launch] [--quit-after <seconds>] [--screenshot <png>]`.
- **The run panel's status follows the frames** of a live session (each header carries its step), not only command
  replies.
- **MIME's coded errors in the console** (MimeConsolePanel, MBCA-COMP-042): MIME 0.3's errors, read from the MIME
  process's stderr or a live-protocol reply's `errors`, are shown in their shape: each reason's class (coloured by
  who can act on it), code, what happened, why, what to do, where, and its documentation page as a link
  (`core/mime_error.h`; MBCA-VER-013 on MIME's own output)
- **StormRenderer** (MBCA-COMP-032): the viewport's Storm render path (engine, camera, lights) without Qt, shared
  with the headless screenshot tests
- **Screenshot tests** (`microbotica_screenshot_tests`): Storm on llvmpipe over EGL, the demo recording compared with
  `tests/fixtures/screenshots/` (MBCA-VER-011), A0's MIME 0.3 recording drawn when `MICROROBOTICA_A0_RUN` names a run
  folder (MBCA-VER-012); `scripts/make_a0_fixtures.sh` writes A0's folders locally

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
- **Profiler**: Lock-free per-frame profiling (`MBCA_PROFILE_SCOPE` macro, `ProfileRingBuffer`, `ProfileReporter`). Zero overhead when `MICROBOTICA_PROFILING` not defined. Debug-only by default.
- **USDC Playback**: Open MIME-recorded `.usdc` files with time-sampled animation. Auto-detects `HasAuthoredTimeCodeRange`, shows timeline slider, plays at recorded fps with scrubbing, looping, and play/pause/stop.
- **RecordingConfig**: `recording.output` field in `experiment.yaml` for pointing to .usdc recording files

### Performance

- **ResultsApplicator**: `SdfChangeBlock` batches all USD attribute writes into a single change notification per frame
- **ResultsApplicator**: Cached `UsdGeomXformOp` handles with generation-counter invalidation — eliminates `GetOrderedXformOps()` per frame
- **LocalViewport**: Cached `GlfSimpleLight`/`GlfSimpleMaterial` as members — only key light position updated per frame
- **ThreadSafeQueue**: `drain_latest()` method — single lock acquisition for entire queue drain (O(1) vs O(N))
- **CMakeLists**: LTO option for release builds, `ENABLE_PROFILING` CMake option

### Security

- **ExperimentLoader**: Replaced `popen()` with `QProcess` for YAML→JSON conversion — prevents command injection via crafted filenames, provides proper exit code and stderr handling
- **ConnectionManager**: Replaced `popen()` with `QProcess` for `sky_resolve.py` — prevents command injection via cluster names

### Fixed

- **CameraToolbar**: `setCameraController()` setter replaces destroy-and-recreate pattern — eliminates double signal connections after scene load

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
