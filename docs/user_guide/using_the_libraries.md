# Using the libraries

MICROROBOTICA is the integrated development environment; the
simulation work itself runs in two Python libraries:

* **[MADDENING](https://microrobotica.org/maddening/)** — the JAX
  graph runtime (`pip install maddening`). Provides the
  `GraphManager`, `SimulationNode` ABC, edge specs, scheduling,
  and the differentiable XLA-compiled step.
* **[MIME](https://microrobotica.org/mime/)** — domain-specific
  microrobotics physics nodes (`pip install mime-engine`). Imports
  as the `mime` package and provides rigid-body, magnetic-response,
  Stokeslet BEM, IB-LBM, FVM-IBM, robot-arm, and actuation nodes.

The IDE spawns MIME's runner as a subprocess and exchanges state over
ZMQ; you don't import either library from C++ directly.

## Install both libraries

```bash
# CPU only
pip install mime-engine

# GPU (CUDA 12)
pip install "mime-engine" "jax[cuda12]"
```

`mime-engine` declares `maddening` as a dependency, so installing one
gets you both.

## Open an experiment from the IDE

The IDE locates MIME's runner via the `PYTHONPATH` of its embedded
interpreter. After `pip install mime-engine` is complete:

1. Launch `microrobotica`.
2. **File → Open Experiment** → pick any `experiment.yaml` from a
   MIME experiment directory (e.g.
   `MIME/experiments/dejongh_confined/experiment.yaml`).
3. The IDE spawns the runner, waits for its ZMQ endpoint, and starts
   streaming results into the viewport.
4. **Simulation → Start** — drag parameter sliders live; the IDE
   forwards them to the runner without rebuilding the graph.
5. The timeline scrubber walks the USDC recording produced by the
   runner.

## Writing your own MIME experiment

A MIME experiment is a directory with three files: `experiment.yaml`,
`physics/setup.py`, and `scene/world.usda`. The runner imports
`physics.setup.build_graph(params)` and feeds the returned
`GraphManager` to the IDE.

```python
# physics/setup.py
from maddening import GraphManager
from mime.nodes.environment.external_magnetic_field import ExternalMagneticFieldNode
from mime.nodes.robot.permanent_magnet_response import PermanentMagnetResponseNode
from mime.nodes.robot.rigid_body import RigidBodyNode


def build_graph(params: dict) -> GraphManager:
    dt = params["DT_PHYS"]

    gm = GraphManager()
    gm.add_node(ExternalMagneticFieldNode(name="field", timestep=dt))
    gm.add_node(PermanentMagnetResponseNode(name="magnet", timestep=dt))
    gm.add_node(RigidBodyNode(name="body", timestep=dt, **params["body"]))

    gm.add_edge("field", "magnet", "field_vector",   "field_vector")
    gm.add_edge("magnet", "body",  "magnetic_torque", "magnetic_torque")

    gm.compile()
    return gm
```

See [`MIME/experiments/`](https://github.com/Microrobotics-Simulation-Framework/MIME/tree/main/experiments)
for fully worked examples — both the
[`dejongh_confined`](https://github.com/Microrobotics-Simulation-Framework/MIME/tree/main/experiments/dejongh_confined)
benchmark and the
[`ar4_helical_drive`](https://github.com/Microrobotics-Simulation-Framework/MIME/tree/main/experiments/ar4_helical_drive)
closed-loop demo.

## Running MIME on its own

You don't have to go through the IDE — MIME is a regular Python
library. Build a `GraphManager`, call `compile()`, and drive it from
Python:

```python
final, history = gm.run_scan_with_history(n_steps=10_000)
```

Full library reference: <https://microrobotica.org/mime/>.
