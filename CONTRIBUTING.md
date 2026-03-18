# Contributing to MICROBOTICA

## Development Environment

There are two ways to set up a development environment:

1. **Docker (recommended)** — pre-built image with all dependencies including OpenUSD
2. **Native** — install dependencies manually on your host machine

## Option 1: Docker Development (Recommended)

The GHCR base image has everything pre-installed: Ubuntu 24.04, GCC 13,
Qt 6, OpenUSD 24.08 (built from source with oneTBB), Catch2, pybind11,
nlohmann-json, spdlog, Doxygen, Sphinx, and all Python packages.

### Step 1: Pull the image

```bash
docker pull ghcr.io/microrobotics-simulation-framework/microrobotica:base
```

This is a ~2.9 GB image. First pull takes 2–5 minutes depending on your
connection.

### Step 2: Start a container

**For tests and headless development (no GUI):**

```bash
docker run -it --rm \
  -v $(pwd):/workspace/microbotica \
  -w /workspace/microbotica \
  -e PXR_ROOT=/opt/usd \
  ghcr.io/microrobotics-simulation-framework/microrobotica:base \
  bash
```

**For GUI development (X11 display forwarding):**

```bash
# First, on your HOST machine (outside Docker), allow local X11 access:
xhost +local:docker

# Then start the container with display forwarding:
docker run -it --rm \
  -v $(pwd):/workspace/microbotica \
  -w /workspace/microbotica \
  -e PXR_ROOT=/opt/usd \
  -e DISPLAY=$DISPLAY \
  -e XDG_RUNTIME_DIR=/tmp/runtime-root \
  -e LIBGL_ALWAYS_SOFTWARE=1 \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  ghcr.io/microrobotics-simulation-framework/microrobotica:base \
  bash
```

> **What the flags mean:**
> - `-v $(pwd):/workspace/microbotica` — mounts your source code into the container
> - `-e PXR_ROOT=/opt/usd` — tells CMake where OpenUSD is installed
> - `-e DISPLAY=$DISPLAY` — forwards your X11 display number
> - `-e LIBGL_ALWAYS_SOFTWARE=1` — uses Mesa software rendering (no GPU passthrough needed)
> - `-v /tmp/.X11-unix:/tmp/.X11-unix` — shares the X11 socket
> - `xhost +local:docker` — allows Docker containers to connect to your X server (run once per session, resets on logout)

### Step 3: Build and run

Inside the container:

```bash
# If you previously built outside Docker, clean first (path mismatch):
rm -rf build/debug

# Configure
cmake --preset linux-debug

# Build everything (app + tests)
cmake --build build/debug

# Run the test suite (32 tests, including USD verification)
cd build/debug && ctest --output-on-failure && cd ../..

# Run the application (requires GUI flags from Step 2)
./build/debug/microbotica

# Run headless (no window, just verifies it starts)
QT_QPA_PLATFORM=offscreen ./build/debug/microbotica &
sleep 2 && kill %1
```

### Important: switching between Docker and native builds

Docker runs as root inside the container, so build files created in Docker
are owned by root on your host. If you switch between Docker and native builds:

```bash
# From Docker: clean the build directory
rm -rf build/

# From host (if Docker created the files): use Docker to clean
docker run --rm -v $(pwd):/w ghcr.io/microrobotics-simulation-framework/microrobotica:base rm -rf /w/build
```

### IDE integration: VS Code + Dev Containers

The repository includes `.devcontainer/devcontainer.json`. No manual setup:

1. Install the [Dev Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) extension
2. Open the repo in VS Code
3. Press `Ctrl+Shift+P` → "Dev Containers: Reopen in Container"
4. VS Code pulls the GHCR image, starts the container, and auto-builds
5. IntelliSense, CMake Tools, Python support are pre-configured
6. First open: ~2–5 minutes (image pull + build). Subsequent: instant.

### IDE integration: CLion

1. **Settings → Build, Execution, Deployment → Toolchains** → add Docker toolchain
2. Image: `ghcr.io/microrobotics-simulation-framework/microrobotica:base`
3. CMake environment: `PXR_ROOT=/opt/usd`
4. CMake preset: `linux-debug`

---

## Option 2: Native Development

### Step 1: Install system packages

On Ubuntu 24.04:

```bash
sudo apt-get install -y \
  build-essential gcc-13 g++-13 cmake \
  qt6-base-dev libqt6openglwidgets6t64 libxkbcommon-dev \
  nlohmann-json3-dev libspdlog-dev catch2 \
  pybind11-dev cppzmq-dev doxygen
```

### Step 2: Install Python packages

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install PyYAML sphinx myst-parser breathe sphinxcontrib-bibtex
```

### Step 3: (Optional) Install OpenUSD

Without OpenUSD, MICROBOTICA compiles and runs but USD-dependent features
(scene loading, viewport rendering, 4 verification tests) are disabled.
All code is guarded with `#ifdef MICROBOTICA_HAS_USD`.

To build OpenUSD from source (~1–2 hours):

```bash
sudo apt-get install -y libboost-all-dev libtbb-dev libglew-dev libgl-dev libx11-dev

git clone --depth 1 --branch v24.08 \
  https://github.com/PixarAnimationStudios/OpenUSD.git /tmp/OpenUSD

cd /tmp/OpenUSD && python3 build_scripts/build_usd.py \
  --no-tests \
  --no-examples \
  --no-tutorials \
  --no-docs \
  --no-python \
  --no-usdview \
  --no-embree \
  --no-prman \
  --no-openimageio \
  --no-opencolorio \
  --no-materialx \
  --onetbb \
  /opt/usd

export PXR_ROOT=/opt/usd
# Add to your ~/.bashrc to persist:
echo 'export PXR_ROOT=/opt/usd' >> ~/.bashrc
```

### Step 4: Build and run

```bash
cmake --preset linux-debug
cmake --build build/debug

# Run tests
cd build/debug && ctest --output-on-failure && cd ../..

# Run the app
./build/debug/microbotica

# Run ASan/UBSan (memory safety checks)
cmake --preset linux-asan
cmake --build build/asan --target microbotica_tests
LSAN_OPTIONS="suppressions=$PWD/lsan.suppressions" ./build/asan/tests/microbotica_tests

# Run compliance scripts
source .venv/bin/activate  # if using native venv
python scripts/check_anomalies.py
python scripts/check_citations.py
python scripts/harvest_component_meta.py
```

---

## CMake Presets

| Preset | Build dir | Purpose |
|--------|-----------|---------|
| `linux-debug` | `build/debug` | Development builds |
| `linux-release` | `build/release` | Optimised builds |
| `linux-asan` | `build/asan` | AddressSanitizer + UndefinedBehaviourSanitizer |
| `linux-tsan` | `build/tsan` | ThreadSanitizer (Phase 1) |

## Time Estimates

| Task | Docker | Native (no USD) | Native (with USD) |
|------|--------|-----------------|-------------------|
| Environment setup | ~5 min (pull) | ~10 min (apt) | ~2 hours (USD build) |
| CMake configure | ~5 sec | ~5 sec | ~5 sec |
| First full build | ~30 sec | ~20 sec | ~30 sec |
| Incremental build | ~5 sec | ~5 sec | ~5 sec |
| Full test suite | ~3 sec | ~3 sec | ~3 sec |
| ASan build + test | ~45 sec | ~35 sec | ~45 sec |
| Compliance scripts | ~2 sec | ~2 sec | ~2 sec |

---

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
