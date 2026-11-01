# Using the libraries

MICROROBOTICA is the integrated development environment; the
simulation work itself runs in two Python libraries:

* **[MADDENING](https://microrobotica.org/maddening/)** — the {term}`JAX`
  graph runtime (`pip install maddening`). Provides the
  `GraphManager`, `SimulationNode` ABC, edge specs, scheduling,
  and the differentiable {term}`XLA`-compiled step.
* **[MIME](https://microrobotica.org/mime/)** — domain-specific
  microrobotics physics nodes (`pip install mime-engine`). Imports
  as the `mime` package and provides rigid-body, magnetic-response,
  {term}`Stokeslet` BEM, {term}`IB-LBM`, FVM-IBM, robot-arm, and actuation nodes.

MICROROBOTICA draws MIME's experiments and their runs; you write and run
experiments in Python with MIME, and never import either library from C++.

## Install both libraries

```bash
pip install mime-engine
```

`mime-engine` declares `maddening` as a dependency, so installing one
gets you both. MIME 0.3 runs on the CPU by default.

## Open a saved experiment

A MIME 0.3 experiment is written in Python and saved as a folder
(`Experiment.save(folder)`, or `mime pull`): `experiment.mime.yaml`, its
lock `mime.lock`, and its stage `scene/scene.usda`, which MIME writes from
the experiment's declarations.

1. Launch `microrobotica`.
2. **File → Open Saved Experiment** → pick the folder. The viewport draws
   its stage with Hydra Storm.

MIME 0.2's experiment folders (`experiment.yaml`, `physics/params.py`, a
hand-made `world.usda`) and its runner (`python -m mime.runner`) are
retired: MICROROBOTICA no longer opens or runs them.

## Play a run's recording

A run folder that `run_to(..., stage_every=...)` or
`mime run <folder> --duration T --out <run> --stage-every S` wrote holds
`recording.usdc`, time samples over the run folder's own copy of the
stage. **File → Open USD Scene** → `recording.usdc` plays it on the
timeline: scrub, play, pause and loop.

Running a saved experiment live, through MIME 0.3's `mime serve`, comes
with MICROROBOTICA's client for its `mime.live/1` protocol.

## The viewport needs no NVIDIA GPU

Storm draws with OpenGL 4.5 in the Compatibility profile, which Mesa
provides on the Intel GPU (`docker run --device /dev/dri`) and on its
software rasteriser, llvmpipe (`LIBGL_ALWAYS_SOFTWARE=1`).
`scripts/run-dev.sh` passes the host's render nodes and never an NVIDIA
GPU's; `MICROROBOTICA_RENDER=llvmpipe` forces the software rasteriser.

## Running MIME on its own

You don't have to go through the IDE: MIME is a regular Python library.
Build an experiment, run it, and save it or its run folder for
MICROROBOTICA:

```python
built = experiment.build()
written = built.run_to("my_run", s(2), stage_every=ms(16.5))  # my_run/recording.usdc
experiment.save("my_experiment")
```

Full library reference: <https://microrobotica.org/mime/>.
