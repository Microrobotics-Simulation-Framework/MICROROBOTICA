# AR4 + Helical-UMR Drive Experiment

This experiment composes a 6-DOF AR4 robot arm (Annin Robotics
open-source) with a magnet-rotor + permanent-magnet end-effector
that drives a helical microrobot in a confined silicone vessel.

All physics runs in JAX and is GPU-ready. The AR4 URDF and STL
meshes are vendored as a stub (see `assets/README.md`); replace with
the upstream files for production-quality rendering.

## Run standalone (no MICROROBOTICA viewer)

## Visualisation-fast vs. publication-fidelity

Two important flags:

* ``--no-coupling-group`` — disables the body↔magnet Gauss-Seidel
  coupling group in favour of staggered back-edges. ~10× faster
  per-step. Trade-off is a one-step phase lag in the magnet's view
  of the microrobot's position (~10° of magnet rotation at 60 Hz
  simulation rate, invisible at the cm-scale robot trajectory).
* The persistent JAX compile cache lives at
  ``~/.cache/jax_compilation_cache``. The **first** run on a clean
  cache pays the full XLA compile (~50 s on a 2060 for the AR4
  graph). Subsequent runs hit the cache and start in ~15 s.

For iteration on the simulation itself, use the standalone runner.
It loads this experiment's `physics/`, builds the graph, steps it
on whatever JAX backend is available, and writes JSON-lines frames
to a file (or stdout). Each frame matches the schema MICROROBOTICA's
`MimePhysicsProcess` expects:

```bash
# Default — runs on GPU when available, CPU otherwise.
.venv/bin/python /home/nick/MSF/MIME/scripts/run_ar4_helical_drive.py \
    --duration 0.5 --frames 100 --out /tmp/ar4_run.jsonl
```

Force a specific backend with `JAX_PLATFORMS=cpu` or
`JAX_PLATFORMS=gpu` if you need to override.

The script reports the JAX backend on the first stderr line, so a
GPU run looks like:

```
# JAX backend: gpu
# JAX devices: [CudaDevice(id=0)]
# graph nodes: ['motor', 'ext_magnet', 'magnet', 'gravity', 'mlp_drag', 'body', 'lub', 'arm']
```

## Run inside MICROROBOTICA

When you are ready to render, point MICROROBOTICA's
`ExperimentLoader` at this directory:

```bash
microrobotica --experiment /path/to/MICROROBOTICA/experiments/ar4_helical_drive
```

The viewer's `MimePhysicsProcess` will spawn the MIME runner as a
subprocess, receive `ResultFrame` JSON over its ZMQ topic, and apply
each `actors[...]` entry to the corresponding USD prim's
`xformOp:translate` / `xformOp:orient` (per
`scene.actors` in `experiment.yaml`).

## Replace the AR4 stub URDF with the real one

```bash
# Clone the upstream AR4 repo and copy URDF + STLs:
git clone https://github.com/Chris-Annin/AR4 /tmp/AR4
cp /tmp/AR4/urdf/ar4.urdf assets/
cp /tmp/AR4/meshes/*.stl assets/meshes/
```

The stub URDF documents the link-name convention the rest of the
experiment expects (`world`, `base_link`, `link_1`..`link_6`). If
the upstream URDF uses different names, update
`physics/params.py:END_EFFECTOR_LINK_NAME` and the `actors:` map in
`experiment.yaml`.

## GPU notes

JAX preallocates GPU memory by default, which can starve cuSolver's
own handle creation and break `RobotArmNode.update`. The
`MIME/tests/conftest.py` already sets the right env vars for tests.
For ad-hoc runs, prepend:

```bash
XLA_PYTHON_CLIENT_PREALLOCATE=false \
XLA_PYTHON_CLIENT_MEM_FRACTION=0.4 \
    .venv/bin/python /path/to/run_ar4_helical_drive.py ...
```
