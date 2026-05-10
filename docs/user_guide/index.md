# User Guide

MICROROBOTICA is the integrated development environment for the
[Microrobotics Simulation Framework](https://microrobotica.org/). It loads MIME experiments
from disk, drives the JAX runner as a managed subprocess over ZMQ,
streams results into a USD-backed viewport, and produces the
IEC-62304-aligned audit trail required for clinical-grade adoption.

## Getting started

The fastest path to a running experiment:

1. **[Install MICROROBOTICA from source](installation.md)** — the
   supported path is the GHCR Docker image
   (`docker pull ghcr.io/microrobotics-simulation-framework/microrobotica:base`)
   plus CMake.
2. **[Install MADDENING + MIME](using_the_libraries.md)** from PyPI
   (`pip install mime-engine`) so the IDE can spawn the runner.
3. Launch `microrobotica` and open one of the bundled experiments via
   **File → Open Experiment** — the IDE spawns the MIME runner in the
   background and waits for its ZMQ endpoint to come up.
4. Hit **Simulation → Start**. Slide parameter values live; the IDE
   forwards them to the runner without rebuilding the graph.
5. Use the timeline to scrub through the resulting USDC recording.

## What's in this section

- [Installation](installation.md) — Docker (recommended) + native
  build paths.
- [Using the libraries](using_the_libraries.md) — installing
  MADDENING and MIME, and writing your own MIME experiment.
- [Concepts](concepts.md) — the three-layer USD composition stack,
  compute backends, and the embedded scripting console.

## Other guides

- [Component Guide](../component_guide/index.md) — the IDE's interfaces
  and concrete implementations, indexed by `MBCA-COMP-XXX` /
  `MBCA-IMPL-XXX` IDs.
- [Validation](../validation/index.md) — verification tests, SOUP
  package, known anomalies.
- [Regulatory](../regulatory/index.md) — IEC 62304, EU MDR, and
  MDCG 2019-11 mapping.

```{toctree}
:hidden:

installation
using_the_libraries
concepts
```
