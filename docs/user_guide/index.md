# User Guide

MICROROBOTICA is the integrated development environment for the
[Microrobotics Simulation Framework](https://microrobotica.org/). It loads MIME experiments
from disk, drives the JAX runner as a managed subprocess over ZMQ,
streams results into a USD-backed viewport, and produces the
IEC-62304-aligned audit trail required for clinical-grade adoption.

## Getting started

The fastest path to a running experiment:

1. Build the IDE — see the
   [README](https://github.com/Microrobotics-Simulation-Framework/MICROROBOTICA#building)
   for the CMake preset walkthrough.
2. Launch `microrobotica` and open one of the bundled experiments via
   **File → Open Experiment** — the IDE spawns the MIME runner in the
   background and waits for its ZMQ endpoint to come up.
3. Hit **Simulation → Start**. Slide parameter values live; the IDE
   forwards them to the runner without rebuilding the graph.
4. Use the timeline to scrub through the resulting USDC recording.

## What's in this section

- [Concepts](concepts.md) — the three-layer USD composition stack,
  compute backends, and the embedded scripting console.

## Other guides

- [Component Guide](../component_guide/index.md) — the IDE's interfaces
  and concrete implementations, indexed by `MBCA-COMP-XXX` /
  `MBCA-IMPL-XXX` IDs.
- [Validation](../validation/index.md) — verification tests, SOUP
  package, known anomalies.
- [Regulatory](../regulatory/intended_use.md) — IEC 62304, EU MDR, and
  MDCG 2019-11 mapping.
