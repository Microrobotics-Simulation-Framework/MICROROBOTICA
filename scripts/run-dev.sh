#!/usr/bin/env bash
# Minimal dev container for MICROROBOTICA — same mounts as the regular
# dev workflow, plus --network host so an `ssh -L` tunnel opened on the
# laptop (forwarding ZMQ 5555/5556 from a cloud cluster to localhost) is
# reachable from inside the container without any Cloud-mode machinery.
#
# Usage:
#   # Terminal 1 (laptop): keep the tunnel alive.
#   ssh -N -L 5555:192.168.2.7:5555 -L 5556:192.168.2.7:5556 mime-old
#
#   # Terminal 2 (laptop): start the container.
#   ~/MSF/msf/MICROROBOTICA/scripts/run-dev.sh
#
#   # Inside the container:
#   cd /workspace/microbotica && ./build/debug/microbotica
#   # Mode = Local autodetects localhost:5555 via the tunnel.
#
# Rendering: Mesa, on the Intel GPU's render nodes (/dev/dri) or on its
# software rasteriser, llvmpipe (MICROROBOTICA_RENDER=llvmpipe forces it).
# No NVIDIA GPU is passed in; see render-args.sh.

set -e

source "$(dirname "$(readlink -f "$0")")/render-args.sh"

MSF=/home/nick/MSF/msf
NAME=microbotica-dev

docker rm -f "$NAME" 2>/dev/null || true

docker run -it --rm \
    --name "$NAME" \
    --user 0:0 \
    --network host \
    "${RENDER_ARGS[@]}" \
    -e DISPLAY="$DISPLAY" \
    -e JAX_PLATFORMS=cpu \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    -v "$MSF/MADDENING":/workspace/maddening \
    -v "$MSF/MIME":/workspace/mime \
    -v "$MSF/MICROROBOTICA":/workspace/microbotica \
    ghcr.io/microrobotics-simulation-framework/microrobotica:base \
    bash
