# Installation (from source)

MICROROBOTICA is a C++17 / Qt 6 desktop application. The supported way
to build it today is **from source** — there is no prebuilt MICROROBOTICA
binary release yet. To keep the toolchain pinned and reproducible, we
publish a Docker image on GHCR that bundles every system dependency
(Qt 6, OpenUSD 24.08, GCC 13, Catch2, pybind11, Doxygen, …) so the only
thing you have to manage is the source tree.

The companion Python libraries
[MADDENING](https://microrobotica.org/maddening/) and
[MIME](https://microrobotica.org/mime/) install separately from PyPI
(see [Using the libraries](using_the_libraries.md)); MICROROBOTICA
spawns MIME's runner as a subprocess at runtime.

## 1. Pull the base image from GHCR

```bash
docker pull ghcr.io/microrobotics-simulation-framework/microrobotica:base
```

This is a ~2.9 GB image. First pull takes 2–5 minutes depending on
your connection.

## 2. Clone the source tree

```bash
git clone --recurse-submodules https://github.com/Microrobotics-Simulation-Framework/MICROROBOTICA
cd MICROROBOTICA
```

## 3. Start a container with your source mounted

For tests and headless development (no GUI):

```bash
docker run -it --rm \
  -v "$PWD":/workspace/microrobotica \
  -w /workspace/microrobotica \
  -e PXR_ROOT=/opt/usd \
  ghcr.io/microrobotics-simulation-framework/microrobotica:base \
  bash
```

For GUI development with X11 forwarding (Linux host):

```bash
# Allow local X11 access (once per session)
xhost +local:docker

docker run -it --rm \
  -v "$PWD":/workspace/microrobotica \
  -w /workspace/microrobotica \
  -e PXR_ROOT=/opt/usd \
  -e DISPLAY="$DISPLAY" \
  -e XDG_RUNTIME_DIR=/tmp/runtime-root \
  -e LIBGL_ALWAYS_SOFTWARE=1 \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  ghcr.io/microrobotics-simulation-framework/microrobotica:base \
  bash
```

What the flags do:

| Flag | Purpose |
|------|---------|
| `-v "$PWD":/workspace/microrobotica` | mounts your source tree into the container |
| `-e PXR_ROOT=/opt/usd` | tells CMake where OpenUSD lives |
| `-e DISPLAY=$DISPLAY` | forwards your X11 display |
| `-e LIBGL_ALWAYS_SOFTWARE=1` | Mesa software rendering (no GPU passthrough needed) |
| `-v /tmp/.X11-unix:/tmp/.X11-unix` | shares the X11 socket |

## 4. Configure, build, and test (inside the container)

```bash
# CMake configure (debug preset)
cmake --preset linux-debug

# Build everything (app + test binaries)
cmake --build build/debug

# Run the test suite (32 tests, including USD verification)
cd build/debug && ctest --output-on-failure && cd ../..

# Run the IDE (requires GUI flags from step 3)
./build/debug/microrobotica
```

Available CMake presets:

| Preset | Build dir | Purpose |
|--------|-----------|---------|
| `linux-debug` | `build/debug` | Development builds |
| `linux-release` | `build/release` | Optimised builds |
| `linux-asan` | `build/asan` | AddressSanitizer + UBSan |
| `linux-tsan` | `build/tsan` | ThreadSanitizer |

## 5. Switching between Docker and native builds

Docker runs as root, so build files created inside the container are
owned by root on your host. If you previously built natively, clean
the build directory before reconfiguring inside Docker (and vice-versa):

```bash
# From inside Docker
rm -rf build/

# From host, when build/ is root-owned (Docker → native switch)
docker run --rm -v "$PWD":/w \
  ghcr.io/microrobotics-simulation-framework/microrobotica:base \
  rm -rf /w/build
```

## 6. Native build (no Docker)

If you'd rather build natively on Ubuntu 24.04, follow the full
walkthrough in
[CONTRIBUTING.md](https://github.com/Microrobotics-Simulation-Framework/MICROROBOTICA/blob/main/CONTRIBUTING.md#option-2-native-development).
Heads-up: building OpenUSD 24.08 from source takes 1–2 hours and is
the slowest step.

## 7. IDE integration

* **VS Code + Dev Containers** — `.devcontainer/devcontainer.json` is
  checked in. Install the Dev Containers extension and choose
  *"Reopen in Container"*.
* **CLion** — add a Docker toolchain pointing at
  `ghcr.io/microrobotics-simulation-framework/microrobotica:base`,
  set `PXR_ROOT=/opt/usd` in the CMake environment, and select the
  `linux-debug` preset.

## 8. First run

Once `./build/debug/microrobotica` launches, open one of the bundled
MIME experiments via **File → Open Experiment**. The IDE will spawn
the MIME Python runner in the background and start streaming results
into the USD viewport. If MIME isn't on the runner's `PYTHONPATH`,
install it from PyPI first:

```bash
pip install mime-engine
```

See [Using the libraries](using_the_libraries.md) for details on how
the IDE drives MIME at runtime.
