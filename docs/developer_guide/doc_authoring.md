# Documentation Authoring Guide

This page demonstrates the authoring primitives available across all three
MSF documentation sub-sites (MICROROBOTICA, MADDENING, MIME).

---

## LaTeX Mathematics

Inline math uses `$…$` delimiters — e.g. the magnetic torque is
$\tau = \mathbf{m} \times \mathbf{B}$.

Block equations use `$$…$$`:

$$
\mathbf{F}_{\text{drag}} = -6\pi\,\eta\,r\,\mathbf{v}
$$

For multi-line aligned equations use the `\begin{align}` environment:

$$
\begin{align}
  \dot{\mathbf{q}} &= f(\mathbf{q}, \mathbf{u}) \\
  \mathbf{y}       &= h(\mathbf{q})
\end{align}
$$

---

## Mermaid Diagrams

Use fenced `mermaid` code blocks anywhere in a Markdown file.
The diagram is rendered client-side by the Mermaid JS library.

### Node-graph topology

The example below mirrors the
[`dejongh_confined`](https://github.com/Microrobotics-Simulation-Framework/MIME/tree/main/experiments/dejongh_confined)
experiment exactly (`graph.json` + `physics/setup.py`) — a useful
template when writing your own node-graph diagrams.

```{mermaid}
flowchart LR
    subgraph MIME["MIME node graph (dejongh_confined)"]
        direction LR
        Field[ExternalMagneticFieldNode<br/>field] -->|field_vector<br/>field_gradient| MagResp[PermanentMagnetResponseNode<br/>magnet]
        Body[RigidBodyNode<br/>body] -->|orientation| MagResp
        MagResp -->|magnetic_force<br/>magnetic_torque| Body
        Gravity[GravityNode<br/>gravity] -->|gravity_force| Body
        Body -->|position / vel / ω| MLP[MLPResistanceNode<br/>mlp_drag]
        MLP -->|resistance_matrix| Lub[LubricationCorrectionNode<br/>lub]
        Body -->|position / vel / ω| Lub
        Lub -->|drag_force<br/>drag_torque| Body
    end
    U1["external: frequency_hz"] -.-> Field
    U2["external: field_strength_mt"] -.-> Field
```

### Sequence diagram

```{mermaid}
sequenceDiagram
    participant IDE as MICROROBOTICA IDE
    participant RT  as MADDENING Runtime
    participant Sim as MIME simulation

    IDE->>RT: load_experiment(path)
    RT->>Sim: build_graph(config)
    Sim-->>RT: compiled_graph
    loop every frame
        IDE->>RT: step(dt)
        RT->>Sim: forward_pass()
        Sim-->>RT: outputs
        RT-->>IDE: render_frame(outputs)
    end
```

---

## Code Snippets

Fenced code blocks get a **copy button** (top-right corner of the block)
automatically.  Shell prompts (`$`) are excluded from the copied text.

### Python

```python
from maddening import GraphManager
from mime.nodes.environment.external_magnetic_field import ExternalMagneticFieldNode
from mime.nodes.robot.permanent_magnet_response import PermanentMagnetResponseNode
from mime.nodes.robot.rigid_body import RigidBodyNode

# Minimal rotating-field → dipole-on-body graph
gm = GraphManager()
gm.add_node(ExternalMagneticFieldNode(name="field", timestep=5e-4))
gm.add_node(PermanentMagnetResponseNode(name="magnet", timestep=5e-4))
gm.add_node(RigidBodyNode(name="body", timestep=5e-4))

gm.add_edge("field", "magnet", "field_vector", "field_vector")
gm.add_edge("magnet", "body", "magnetic_torque", "magnetic_torque")

gm.compile()
final, history = gm.run_scan_with_history(n_steps=1000)
print(history["body"]["orientation"].shape)  # (1000, 4)
```

### Shell

```bash
$ cd MADDENING
$ pip install -e ".[dev]"
$ pytest tests/ -x -q
```

### YAML configuration

This is the shape of `experiment.yaml` files MIME experiments expose
to MICROROBOTICA's runner (see `MIME/experiments/dejongh_confined/`
for the live version):

```yaml
schema_version: "1.0"
mime_version_min: "0.2.0"

physics:
  setup: physics/setup.py
  params: physics/params.py

control:
  controller: control/controller.py

scene:
  world: scene/world.usda
  actors:
    body:
      prim_path: /World/Actors/UMR
      prim_type: Mesh
      state_fields: [position, orientation]
```

---

## Admonitions

Use MyST admonitions for tips, notes, warnings, and danger notices:

```{note}
This is a **note** — background context or supplementary information.
```

```{tip}
This is a **tip** — a recommended practice or efficiency hint.
```

```{warning}
This is a **warning** — something that could cause unexpected behaviour.
```

```{important}
This is an **important** notice — must-know information before proceeding.
```

---

## Cross-project references

Use `intersphinx` to link across sub-sites:

```rst
See the :doc:`MADDENING node-authoring guide <maddening:developer_guide/node_authoring>`.
```

Or in Markdown:

```markdown
See the [node authoring guide](maddening:developer_guide/node_authoring).
```
