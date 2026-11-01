# mime.live/1 messages captured from MIME 0.3

Captured on 2026-11-01 from a real `mime serve` of A0, MIME's reference experiment, saved with its controller in the
callback mode (`scripts/make_a0_fixtures.sh`, `a0_callback_saved`), at MIME's `v0.3-phase7n` `578cb0b`, whose tree
MIME's `v0.3` holds unchanged since PR #86 merged (`21e98f5`); first captured at `3136fb1`. Local paths are
replaced by `/data/...`. The tests in `tests/test_live_protocol.cpp` read them.

| File | What it is |
|---|---|
| `hello_a0_callback.json` | the `hello` reply: experiment, controller, live inputs, stage, visuals, binding, topology, schema |
| `replies_a0_callback.json` | replies to `status`, `step`, `set` (accepted and refused), an unknown command, `record`, `start`, `pause`, `pace`, `stop` |
| `ready.json` | what `mime serve --ready-file` wrote |
| `frame_0.bin`, `frame_1.bin` | binary frames 0 (the starting state) and 1 (after one 40-step chunk) |
| `frame_last.bin` | the session's last frame, its final flag set |
| `frame_1.json` | frame 1 of a second session of the same folder, with `--json-frames` |

When MIME's protocol changes, capture these again (`scripts/capture_live_fixtures.py`) and update
`src/live/live_protocol.*` to match.
