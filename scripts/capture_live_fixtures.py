"""Capture the mime.live/1 messages tests/fixtures/live/ holds, from a real `mime serve` of A0 saved with its controller
called back (scripts/make_a0_fixtures.sh writes `a0_callback_saved`).

Run with MIME's Python, from anywhere:

    <MIME>/.venv/bin/python scripts/capture_live_fixtures.py <a0_callback_saved> <scratch dir> tests/fixtures/live

It serves the folder twice (binary frames, then JSON frames), records hello, a few frames and the replies to each
command, and writes them into the fixtures folder with every local path replaced by /data/... . Re-run it when MIME's
client note lists "Changes since the first note", then update src/live/live_protocol.* to match.
"""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
import time
from pathlib import Path

from mime.live import LiveClient
from mime.live._protocol import is_frame

saved, scratch, fixtures = (Path(a).resolve() for a in sys.argv[1:4])
scratch.mkdir(parents=True, exist_ok=True)
fixtures.mkdir(parents=True, exist_ok=True)


def serve(tag: str, *extra: str) -> tuple[subprocess.Popen, dict]:
    ready = scratch / f"ready_{tag}.json"
    ready.unlink(missing_ok=True)
    process = subprocess.Popen([sys.executable, "-m", "mime.cli", "serve", str(saved), "--commands",
                                "tcp://127.0.0.1:0", "--frames", "tcp://127.0.0.1:0", "--ready-file", str(ready),
                                "--out", str(scratch / f"run_{tag}"), *extra],
                               stdout=(scratch / f"serve_{tag}.out").open("w"),
                               stderr=(scratch / f"serve_{tag}.err").open("w"))
    while not ready.exists():
        assert process.poll() is None, (scratch / f"serve_{tag}.err").read_text()
        time.sleep(0.05)
    return process, json.loads(ready.read_text())


def clean(text: str) -> str:
    text = text.replace(str(saved), "/data/a0_callback_saved")
    text = text.replace(str(scratch / "run_bin"), "/data/runs/script_a0-live")
    return text.replace(str(scratch), "/data")


process, endpoints = serve("bin")
client = LiveClient(endpoints["commands"], endpoints["frames"])
replies = {"status": client.request("status"), "step": client.request("step", chunks=3)}
frames = [client.frames.receive() for _ in range(4)]
replies["set"] = client.request("set", values={"offset": 0.002})
replies["set_refused"] = client.request("set", values={"gain": 1.0})
replies["unknown_refused"] = client.request("jump")
replies["record_on"] = client.request("record", on=True)
replies["start"] = client.request("start")
time.sleep(0.5)
replies["pause"] = client.request("pause")
replies["pace"] = client.request("pace", rtf=None)
replies["stop"] = client.request("stop")
last = None
try:
    while True:
        last = client.frames.receive()
except Exception:  # the session closed its sockets
    pass
hello = client.hello
client.close()
assert process.wait(timeout=120) == 0

(fixtures / "hello_a0_callback.json").write_text(json.dumps(json.loads(clean(json.dumps(hello))), indent=1) + "\n")
(fixtures / "replies_a0_callback.json").write_text(
    json.dumps(json.loads(clean(json.dumps(replies))), indent=1) + "\n")
(fixtures / "ready.json").write_text(json.dumps(json.loads(clean(json.dumps(endpoints))), indent=1) + "\n")
for number in (0, 1):
    assert is_frame(frames[number])
    (fixtures / f"frame_{number}.bin").write_bytes(frames[number])
assert last is not None and is_frame(last)
(fixtures / "frame_last.bin").write_bytes(last)

process, endpoints = serve("json", "--json-frames")
client = LiveClient(endpoints["commands"], endpoints["frames"])
client.request("step", chunks=1)
json_frames = [client.frames.receive() for _ in range(2)]
(fixtures / "frame_1.json").write_bytes(json_frames[1])
client.request("stop")
client.close()
assert process.wait(timeout=120) == 0
shutil.rmtree(scratch / "run_bin", ignore_errors=True)
shutil.rmtree(scratch / "run_json", ignore_errors=True)
print("captured into", fixtures)
