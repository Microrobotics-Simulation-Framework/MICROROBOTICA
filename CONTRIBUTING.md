# Contributing to MICROBOTICA

## Development Environment

There are two ways to set up a development environment:

1. **Docker (recommended)** — uses a pre-built image with all dependencies including OpenUSD
2. **Native** — install all dependencies manually on your host

### Option 1: Docker Development (Recommended)

The GHCR base image contains Ubuntu 24.04, GCC 13, Qt 6, OpenUSD 24.08 (built from source), and all other dependencies. This is the fastest way to get a working environment (~2 minutes to pull the image, vs. 1–2 hours to build USD from source).

#### Pull the image

```bash
docker pull ghcr.io/microrobotics-simulation-framework/microrobotica:base
```

This is a ~2.9 GB image. First pull takes 2–5 minutes on a fast connection.

#### Interactive development (terminal)

Mount your source tree into the container and work interactively:

```bash
docker run -it --rm \
  -v $(pwd):/workspace/microbotica \
  -w /workspace/microbotica \
  -e PXR_ROOT=/opt/usd \
  ghcr.io/microrobotics-simulation-framework/microrobotica:base \
  bash
```

Inside the container:

```bash
cmake --preset linux-debug        # ~5 seconds
cmake --build build/debug         # ~30 seconds first build
./build/debug/tests/microbotica_tests  # ~3 seconds, all tests including USD
```

> **Note**: The build directory is created inside the mounted volume, so build
> artifacts persist between container restarts. If you switch between native
> and Docker builds, delete `build/` first — the paths will differ.

#### GUI development (with display forwarding)

To run the MICROBOTICA application with a GUI from Docker:

```bash
# Linux (X11)
docker run -it --rm \
  -v $(pwd):/workspace/microbotica \
  -w /workspace/microbotica \
  -e PXR_ROOT=/opt/usd \
  -e DISPLAY=$DISPLAY \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  --device /dev/dri \
  ghcr.io/microrobotics-simulation-framework/microrobotica:base \
  bash

# Inside container:
cmake --preset linux-debug && cmake --build build/debug
./build/debug/microbotica
```

> **Note**: `--device /dev/dri` passes the GPU through for OpenGL rendering.
> On Wayland, you may need `-e WAYLAND_DISPLAY=$WAYLAND_DISPLAY -v $XDG_RUNTIME_DIR/$WAYLAND_DISPLAY:/tmp/$WAYLAND_DISPLAY` instead of the X11 socket.

#### IDE integration: VS Code + Dev Containers

The Docker image works with VS Code's Dev Containers extension for a full IDE experience:

1. Install the [Dev Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) extension
2. Create `.devcontainer/devcontainer.json` in the repo root:

```json
{
    "name": "MICROBOTICA",
    "image": "ghcr.io/microrobotics-simulation-framework/microrobotica:base",
    "containerEnv": {
        "PXR_ROOT": "/opt/usd"
    },
    "customizations": {
        "vscode": {
            "extensions": [
                "ms-vscode.cpptools",
                "ms-vscode.cmake-tools",
                "ms-python.python"
            ],
            "settings": {
                "cmake.configureArgs": ["--preset", "linux-debug"],
                "C_Cpp.default.compileCommands": "${workspaceFolder}/build/debug/compile_commands.json"
            }
        }
    },
    "workspaceMount": "source=${localWorkspaceFolder},target=/workspace/microbotica,type=bind",
    "workspaceFolder": "/workspace/microbotica"
}
```

3. Open the repo in VS Code, press `Ctrl+Shift+P` → "Dev Containers: Reopen in Container"
4. VS Code will start inside the container with IntelliSense, CMake Tools, and debugging configured
5. First build takes ~30 seconds; subsequent builds are incremental (~5 seconds)

#### IDE integration: CLion

CLion supports Docker toolchains natively:

1. Go to **Settings → Build, Execution, Deployment → Toolchains**
2. Add a new **Docker** toolchain pointing to `ghcr.io/microrobotics-simulation-framework/microrobotica:base`
3. Set the CMake environment variable `PXR_ROOT=/opt/usd`
4. Set the CMake preset to `linux-debug`
5. CLion will sync files, run CMake, and provide full code navigation with USD headers resolved

### Option 2: Native Development

#### Prerequisites

- **C++ compiler**: GCC 13+ or Clang 16+ with C++17 support
- **CMake**: 3.25+
- **Qt**: 6.4+ (Core, Widgets, OpenGLWidgets)
- **OpenUSD**: 24.08+ (optional — USD-dependent features are guarded with `#ifdef MICROBOTICA_HAS_USD`)
- **pybind11**: 2.11+
- **nlohmann_json**: 3.11.0+ (required for `std::optional` serialization)
- **spdlog**: 1.12+
- **Catch2**: 3.4+

On Ubuntu 24.04:

```bash
sudo apt-get install -y \
  build-essential gcc-13 g++-13 cmake \
  qt6-base-dev libqt6openglwidgets6t64 \
  nlohmann-json3-dev libspdlog-dev catch2 \
  pybind11-dev cppzmq-dev doxygen
```

For Python compliance scripts:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install PyYAML sphinx myst-parser breathe sphinxcontrib-bibtex
```

> **OpenUSD** is not available via apt. Without it, MICROBOTICA compiles and
> runs but USD-dependent features (scene loading, layer separation verification
> tests) are disabled. To get full USD support, use the Docker image or build
> OpenUSD from source (~1–2 hours):
> ```bash
> git clone --depth 1 --branch v24.08 https://github.com/PixarAnimationStudios/OpenUSD.git /tmp/OpenUSD
> cd /tmp/OpenUSD && python3 build_scripts/build_usd.py \
>     --no-tests --no-examples --no-tutorials --no-docs --no-python \
>     --no-usdview --no-embree --no-prman --no-openimageio --no-opencolorio \
>     --no-materialx /opt/usd
> export PXR_ROOT=/opt/usd
> ```

#### Build

```bash
cmake --preset linux-debug
cmake --build build/debug
```

#### Run Tests

```bash
cmake --build build/debug --target microbotica_tests
cd build/debug && ctest --output-on-failure
```

#### Run the Application

```bash
./build/debug/microbotica
```

#### Run ASan/UBSan

```bash
cmake --preset linux-asan
cmake --build build/asan --target microbotica_tests
LSAN_OPTIONS=suppressions=lsan.suppressions ./build/asan/tests/microbotica_tests
```

#### Run Compliance Scripts

```bash
source .venv/bin/activate  # if using native venv
python scripts/check_anomalies.py
python scripts/check_citations.py
python scripts/harvest_component_meta.py
```

## Time Estimates

| Task | Docker | Native (no USD) | Native (with USD) |
|------|--------|-----------------|-------------------|
| Environment setup | ~5 min (pull image) | ~10 min (apt install) | ~2 hours (build USD) |
| First CMake configure | ~5 sec | ~5 sec | ~5 sec |
| First full build | ~30 sec | ~20 sec | ~30 sec |
| Incremental build (1 file) | ~5 sec | ~5 sec | ~5 sec |
| Full test suite | ~3 sec | ~3 sec | ~3 sec |
| ASan build + test | ~45 sec | ~35 sec | ~45 sec |
| Compliance scripts | ~2 sec | ~2 sec | ~2 sec |

## Commit Convention

| Prefix | When to use |
|---|---|
| `feat:` | New feature or capability |
| `fix:` | Bug fix |
| `refactor:` | Code restructuring (no behaviour change) |
| `docs:` | Documentation-only changes |
| `test:` | Test additions or changes |
| `perf:` | Performance improvement |
| `verify:` | Verification/validation evidence |
| `break:` | Breaking API change |
| `deprecate:` | Deprecation notice |
| `security:` | Security-relevant change |
| `ui:` | UI/UX change |

The commit message body should explain **why**, not what (the diff shows what).
Keep the subject line under 72 characters.

## ID Prefix Convention

All MICROBOTICA identifiers use the `MBCA-` prefix:

| ID format | Purpose |
|---|---|
| `MBCA-COMP-XXX` | Abstract interface component IDs |
| `MBCA-IMPL-XXX` | Concrete implementation component IDs |
| `MBCA-ANO-XXX` | Known anomaly entries |
| `MBCA-VER-XXX` | Verification benchmark IDs |

## Adding New Components

See `.claude/skills/new-component/SKILL.md` for the full procedure.

## Adding Anomaly Entries

See `.claude/skills/new-anomaly/SKILL.md` for the full procedure.

## Pre-Commit Checklist

See `.claude/skills/commit-and-push/SKILL.md` for the full checklist.
