# Architecture — MICROBOTICA

> This is a living document. It describes the architecture as currently
> implemented. It will be updated as the project evolves.

## System Context

MICROBOTICA is Layer 3 in a four-layer open-source stack for medical
microrobotics simulation:

```
Layer 1 — MADDENING   (Python/JAX, LGPL-3.0)    Physics framework
Layer 2 — MIME         (Python, LGPL-3.0)         Microrobotics engine
Layer 3 — MICROBOTICA  (C++17/Qt 6, AGPL-3.0)    Simulator UI  ← this project
Layer 4 — Commercial   (future)                   CE-marked SaMD
```

MICROBOTICA does not produce physics data. It receives simulation results
from upstream layers (via `PhysicsProcess`), displays them in a viewport,
and provides a Python scripting console for interaction. Its correctness
obligation is **faithful rendering**: data received must be displayed
without corruption or silent modification.

## High-Level Module Map

```
src/
├── core/          Zero-dependency interfaces and data types
├── stubs/         Stub implementations for testing
├── scene/         USD three-layer scene management
├── simulation/    SimulationController and async polling
├── scripting/     Embedded Python interpreter and microrobotica module
├── viewport/      OpenGL/software viewport rendering
├── panels/        Qt dock widgets (hierarchy, properties, timeline, console)
└── app/           Application shell (QApplication, MainWindow)
```

## Module Dependencies

```
core/  ←──────────────────────────────────────────┐
  │  (no Qt, no USD, no Python)                   │
  │  nlohmann_json + spdlog only                  │
  ▼                                               │
stubs/  ←── implements core/ interfaces            │
  │                                               │
  ▼                                               │
scene/  ←── uses core/ types + USD (optional)     │
  │                                               │
  ▼                                               │
simulation/ ←── uses core/ + scene/               │
  │                                               │
  ▼                                               │
scripting/ ←── uses core/ + pybind11              │
  │                                               │
  ▼                                               │
viewport/ ←── uses core/ + Qt + USD (optional)    │
  │                                               │
  ▼                                               │
panels/ ←── uses core/ + scene/ + simulation/ + scripting/
  │                                               │
  ▼                                               │
app/ ←── wires everything together                │
  │  owns: SceneManager, SimulationController,    │
  │        ScriptingEngine, all panels, viewport  │
  └───────────────────────────────────────────────┘
```

The critical rule: `src/core/` has **zero** Qt, USD, or Python dependencies.
It compiles against only the C++ standard library, nlohmann_json, and spdlog.
This is enforced by the `microbotica_core` CMake target's limited include
directories.

## Key Abstractions

### Component System

Every significant class has a `ComponentMeta` struct (`src/core/component_meta.h`)
containing structured metadata: component ID, version, stability level,
preconditions, postconditions, invariants, hazard hints, and validated regimes.
This metadata is machine-readable (JSON-serializable) and harvested by
`scripts/harvest_component_meta.py` for compliance reports.

| ID | Class | Location |
|----|-------|----------|
| MBCA-COMP-001 | PhysicsProcess | `src/core/physics_process.h` |
| MBCA-COMP-002 | RenderBackend | `src/core/render_backend.h` |
| MBCA-COMP-003 | RenderSession | `src/core/render_session.h` |
| MBCA-COMP-004 | ComputeBackend | `src/core/compute_backend.h` |
| MBCA-COMP-010 | SceneManager | `src/scene/scene_manager.h` |
| MBCA-COMP-011 | ResultsApplicator | `src/scene/results_applicator.h` |
| MBCA-COMP-020 | SimulationController | `src/simulation/simulation_controller.h` |
| MBCA-COMP-030 | ViewportWidget | `src/viewport/viewport_widget.h` |
| MBCA-COMP-040 | microrobotica module | `src/scripting/microbota_module.cpp` |
| MBCA-COMP-041 | ConsoleWidget | `src/panels/console_widget.h` |
| MBCA-IMPL-001 | StubPhysicsProcess | `src/stubs/stub_physics_process.h` |
| MBCA-IMPL-002 | LocalComputeBackend | `src/stubs/local_compute_backend.h` |

### USD Three-Layer Composition

SceneManager enforces a three-layer USD composition stack:

```
Session Layer (anonymous root — composes sublayers)
  ├── Results Layer (strongest — simulation output, never persisted)
  ├── Override Layer (middle — user scripting writes)
  └── Base Layer (weakest — original scene file, immutable after load)
```

This ensures simulation output cannot contaminate the original scene, and
user overrides are separable from both.

### Async Simulation Architecture

```
┌─────────────────┐         ┌──────────────────┐
│  Qt Main Thread  │         │  std::async      │
│                  │         │  Worker Thread    │
│  requestNext─────┼── poll ─┤                  │
│  Frame()         │         │  receiveResult() │
│                  │◄─ push ─┤  (blocking)      │
│  frameReady()    │         │                  │
│  signal          │  queue  │  PhysicsProcess  │
└─────────────────┘         └──────────────────┘
```

`SimulationController` bridges the blocking `PhysicsProcess::receiveResult()`
and the non-blocking Qt event loop using `std::async` + `ThreadSafeQueue`.
The Qt main thread polls via `requestNextFrame()` driven by a 60 Hz `QTimer`.

## Build System

CMake 3.25+ with presets. Key targets:

| Target | What it builds |
|--------|---------------|
| `microbotica_core` | Static library: `src/core/` only. No Qt/USD/Python. |
| `microbotica` | Qt application executable. Links everything. |
| `microbotica_tests` | Catch2 test binary. 32 test cases. |
| `microrobotica_py` | Standalone pybind11 module (optional, `BUILD_PYTHON_MODULE=ON`). |

OpenUSD is optional. When not found, `MICROBOTICA_HAS_USD` is not defined
and all USD code paths compile to no-ops. The application still runs with
stub physics and a software viewport.

## Testing

Three tiers:

1. **Unit tests** (`tests/test_*.cpp`, tag `[unit]`) — implementation correctness
2. **Verification tests** (`tests/verification/`, tag `[verification]`) — safety-relevant properties, registered via `REGISTER_VERIFICATION_BENCHMARK` macro
3. **Integration tests** (tag `[integration]`) — multi-component end-to-end

Memory safety: ASan+UBSan on every PR, TSan nightly (Phase 1), Valgrind weekly (Phase 1).

## CI/CD

GitHub Actions with two jobs running inside the GHCR Docker base image:

- **build-test**: configure, build, test, compliance scripts
- **asan-ubsan**: ASan build + test with leak suppressions

The Docker base image is rebuilt automatically when `docker/Dockerfile.base`
changes.

## Regulatory Context

MICROBOTICA is **not** a medical device. When used inside a regulated product,
it is classified as IEC 62304 SOUP. The architecture supports this via:

- `ComponentMeta` on all interfaces (machine-harvestable)
- `known_anomalies.yaml` (4 seed entries with safety relevance rationale)
- Verification benchmarks (6 registered, linked to component IDs)
- `docs/regulatory/intended_use.md` (EU MDR boundary language)
- `docs/validation/soup_package.md` (IEC 62304 SOUP assessment support)

See `DOCUMENTATION_ARCHITECTURE.md` for the full regulatory documentation plan.
