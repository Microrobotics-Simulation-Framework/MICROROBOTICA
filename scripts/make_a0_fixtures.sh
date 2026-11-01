#!/usr/bin/env bash
# A0, MIME 0.3's reference experiment, as the folders MICROROBOTICA's tests and a developer's session use:
#
#   <out>/a0_saved/            A0's saved folder (its controller inside the compiled step): `mime serve` serves it
#   <out>/a0_callback_saved/   the same with its controller called back every 10 ms, so its live input (`offset`) is
#                              settable through the live-inputs panel
#   <out>/a0_run/              a 1.98 s run folder with its recording.usdc (a sample every 16.5 ms), for playback and
#                              the screenshot test (MICROROBOTICA_A0_RUN=<out>/a0_run)
#
# Usage: scripts/make_a0_fixtures.sh <MIME checkout, set up with `uv sync`> <out dir>
#
# Built from MIME's own A0 script (tests/script_a0.py), with MIME's Python, on the CPU. Takes about a minute. The
# folders stay local: never commit them (a recording is a run's, and the AR4's meshes must never be committed in any
# form; these folders hold none, but the rule is kept simple: design #198).
set -euo pipefail

MIME=$(cd "${1:?usage: make_a0_fixtures.sh <MIME checkout> <out dir>}" && pwd)
OUT=${2:?usage: make_a0_fixtures.sh <MIME checkout> <out dir>}
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
PY="$MIME/.venv/bin/python"
[ -x "$PY" ] || { echo "no $PY: run 'uv sync --locked' in $MIME first" >&2; exit 2; }

cd "$MIME"
JAX_PLATFORMS=cpu PYTHONPATH="$MIME/tests" nice -n 10 "$PY" - "$OUT" <<'PY'
import shutil
import sys
import time
import warnings
from pathlib import Path

from script_a0 import a0

from mime.units import ms, s

out = Path(sys.argv[1])
with warnings.catch_warnings():
    warnings.simplefilter("ignore")
    for name, settings in (("a0_saved", {}), ("a0_callback_saved", {"mode": "callback", "rate": ms(10.0)})):
        folder = out / name
        if folder.exists():
            shutil.rmtree(folder)
        started = time.perf_counter()
        a0(**settings).save(folder)
        print(f"{name}: saved in {time.perf_counter() - started:.1f} s", flush=True)
    folder = out / "a0_run"
    if folder.exists():
        shutil.rmtree(folder)
    started = time.perf_counter()
    written = a0().build().run_to(folder, s(1.98), stage_every=ms(16.5))  # 120 chunks of 33 steps
    print(f"a0_run: {written.recording} in {time.perf_counter() - started:.1f} s", flush=True)
PY
