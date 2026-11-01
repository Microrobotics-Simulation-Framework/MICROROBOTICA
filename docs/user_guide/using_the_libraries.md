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

## Open a run folder

A run folder is what `run_to(...)`, `mime run` or a live session wrote:
its experiment (`experiment/`), its outputs (`outputs.json`,
`outputs.npz`), its record (`run.json`), its topology and, when the run
recorded its stage (`stage_every=...`, `--stage-every`, or **Record**
during a session), `recording.usdc`: time samples over the run folder's
own copy of the stage.

**File → Open Run Folder** (or `microbotica <run folder>`) opens it whole:

- the recording plays on the timeline (scrub, play, pause, loop, and a
  speed from 0.1x to 4x), or the experiment's stage is shown when there
  is none;
- the **Plots** panel shows what the run recorded per step
  (`record=[...]`, `every=...`), in its units, flags shaded where raised,
  with a cursor that follows playback;
- the **Claims** panel shows what MIME says of the run: the validity lines
  it was watched against, its flags and its profile's pending items;
- the graph inspector shows the graph it ran; Launch runs its experiment
  again, live; and **Simulation → Replay This Session** replays a live
  session from its command log into a new run folder.

## Run a saved experiment live

With a saved experiment open, **Launch** (Run Configuration, mode Local)
runs `mime serve <folder>` and connects to it (MIME's `mime.live/1`
protocol). Name the `mime` command in **File → Settings → MIME command**
when it is not on `PATH` (for example `<MIME checkout>/.venv/bin/mime`), or
set `MICROROBOTICA_MIME`.

- The viewport moves the stage's prims as MIME's stage binding says, in the
  stage's units; a field vector draws as an arrow at the body.
- **Pause**, **Start**, **Simulation → Step One Chunk** (F8) and
  **Simulation → Record** act on the session; **Stop** ends it, and the
  status bar names the run folder it wrote (under **Settings → Run
  folders**): its experiment, command log, outputs and, when recorded,
  `recording.usdc`.
- The **Live Inputs** panel lists what the controller declares settable
  live (A0 called back: `offset`, metres along the tube), sent with **Set**;
  the experiment's settings are shown read-only, since changing one is a
  rebuild.
- The **Graph Inspector** shows the built graph (`topology()`), and the
  **MIME Output** panel shows `mime serve`'s output and any refusal in
  MIME's shape.
- A remote session: run `mime serve` there, open an SSH tunnel
  (`ssh -L 5555:localhost:5555 -L 5556:localhost:5556 <host>`), open the
  same saved experiment here, and connect with mode **Manual endpoint**.

The AR4's meshes are Annin Robotics' design and are never shipped: the arm
draws as simple shapes until `mime visuals fetch ar4_mk5` puts them in
MIME's asset cache, from which the session's overlay draws them.

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
