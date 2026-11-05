#!/usr/bin/env bash
# The viewport's OpenGL on this machine's display: the GUI tests (MBCA-VER-020, -021) in the base image, through Qt's
# xcb platform and GLX as the app draws on a desktop, on the Intel GPU through Mesa by default or on llvmpipe
# (MICROROBOTICA_RENDER=llvmpipe; render-args.sh, never the NVIDIA GPU). A Core context, a GL error Storm reports or an
# empty frame fails them; each test's window shows for a few seconds. CI runs the same tests under Xvfb.
#
# Usage:  scripts/check-display.sh [build dir, default build/debug] [-- extra Catch2 arguments]
#   MICROROBOTICA_A0_RUN=<a MIME 0.3 run folder> also draws A0's recording (scripts/make_a0_fixtures.sh writes one).
#   MICROROBOTICA_IMAGE=<image> overrides the base image (default: docker/base-image's).

set -euo pipefail

ROOT=$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd)
BUILD=${1:-build/debug}
shift || true
[ "${1:-}" = "--" ] && shift
BIN="$ROOT/$BUILD/tests/microbotica_gui_tests"
[ -x "$BIN" ] || { echo "no $BIN: build the microbotica_gui_tests target first" >&2; exit 2; }
[ -n "${DISPLAY:-}" ] || { echo "DISPLAY is not set: this checks the viewport on a desktop's display" >&2; exit 2; }
IMG=${MICROROBOTICA_IMAGE:-$(tr -d '[:space:]' < "$ROOT/docker/base-image")}

source "$ROOT/scripts/render-args.sh"
GROUPS_ARGS=()
for node in /dev/dri/renderD* /dev/dri/card*; do [ -e "$node" ] && GROUPS_ARGS+=(--group-add "$(stat -c %g "$node")"); done
A0=()
if [ -n "${MICROROBOTICA_A0_RUN:-}" ]; then
    A0=(-e MICROROBOTICA_A0_RUN="$MICROROBOTICA_A0_RUN" -v "$MICROROBOTICA_A0_RUN":"$MICROROBOTICA_A0_RUN":ro)
fi

echo "viewport check on DISPLAY=$DISPLAY (${MICROROBOTICA_RENDER:-auto}: ${RENDER_ARGS[*]}), image $IMG"
exec docker run --rm --user "$(id -u):$(id -g)" "${GROUPS_ARGS[@]}" "${RENDER_ARGS[@]}" \
    -e DISPLAY="$DISPLAY" -e QT_X11_NO_MITSHM=1 -e QT_QPA_PLATFORM=xcb -v /tmp/.X11-unix:/tmp/.X11-unix \
    -e HOME=/tmp "${A0[@]}" -v "$ROOT":"$ROOT":ro -w "$ROOT" \
    "$IMG" "$BIN" --reporter console --success "$@"
