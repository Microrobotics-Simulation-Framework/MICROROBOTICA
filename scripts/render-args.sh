#!/usr/bin/env bash
# Sourced by the run scripts: the `docker run` arguments that give MICROROBOTICA's viewport its OpenGL, through Mesa.
#
#   MICROROBOTICA_RENDER=auto      (default) the host's GPU render nodes through Mesa (the Intel GPU on the laptop),
#                                  or Mesa's software rasteriser (llvmpipe) when there are none
#   MICROROBOTICA_RENDER=llvmpipe  llvmpipe always (reproducible pictures, as CI draws them)
#
# Storm needs OpenGL 4.5 Compatibility, which Mesa gives on both. An NVIDIA GPU's nodes are never passed in, whichever
# driver holds them (the laptop's NVIDIA driver deadlocks; the viewport needs no NVIDIA anything). Usage:
#   source "$(dirname "$0")/render-args.sh"; docker run ... "${RENDER_ARGS[@]}" ...

render_args() {
    if [ "${MICROROBOTICA_RENDER:-auto}" = "llvmpipe" ]; then
        printf '%s\n' -e LIBGL_ALWAYS_SOFTWARE=1
        return
    fi
    local node name driver found=0
    for node in /dev/dri/card* /dev/dri/renderD*; do
        [ -e "$node" ] || continue
        name=$(basename "$node")
        driver=$(basename "$(readlink -f "/sys/class/drm/$name/device/driver" 2>/dev/null)" 2>/dev/null || true)
        case "$driver" in
            nvidia* | nouveau) continue ;;  # never the NVIDIA GPU
        esac
        printf '%s\n' --device "$node"
        found=1
    done
    if [ "$found" = 0 ]; then
        printf '%s\n' -e LIBGL_ALWAYS_SOFTWARE=1
    fi
}

mapfile -t RENDER_ARGS < <(render_args)
