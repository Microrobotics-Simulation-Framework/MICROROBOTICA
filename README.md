# MICROBOTICA

MICROROBOTs Iterative Simulation for Clinical Adoption.

## What is MICROBOTICA?

MICROBOTICA is an open-source research simulator for microrobot-assisted
drug delivery in cerebrospinal fluid and other confined biological
geometries. Built on C++17/Qt 6 with OpenUSD as the primary scene
format, MICROBOTICA provides a desktop application for interactive
visualisation, scripting, and analysis of microrobotics simulations.

MICROBOTICA sits at Layer 3 of an open-source stack: MADDENING (physics
framework) → MIME (microrobotics engine) → MICROBOTICA (simulator UI).

## Intended Use and Disclaimers

> **MICROBOTICA is research software.** It is not a medical device as
> defined by EU MDR (EU 2017/745) or US FDA regulations. It has no
> medical purpose. It is not intended for clinical use, clinical
> decision-making, or patient diagnosis. It has not been CE-marked,
> cleared, or approved by any regulatory body.
>
> When used as a component within regulated medical software,
> MICROBOTICA is classified as SOUP (Software of Unknown Provenance)
> under IEC 62304. The device manufacturer is responsible for
> assessing MICROBOTICA's suitability and performing all required
> verification and validation. See
> [Regulatory Documentation](docs/regulatory/) for details.

## Quick Start

```bash
# Prerequisites: Qt 6, OpenUSD 24.08, CMake 3.25+, GCC 13+
cmake --preset linux-debug
cmake --build build/debug
./build/debug/microbotica
```

## Documentation

| Document | Description |
|----------|-------------|
| [User Guide](docs/user_guide/) | Installation, tutorials, concepts |
| [Component Guide](docs/component_guide/) | Architectural component docs |
| [Developer Guide](docs/developer_guide/) | Contributing, testing, code style |
| [Validation](docs/validation/) | V&V evidence, SOUP package |
| [Regulatory](docs/regulatory/) | Intended use, EU MDR/FDA guidance |
| [CHANGELOG](CHANGELOG.md) | Version history |

## Citation

If you use MICROBOTICA in academic work, please cite:

```bibtex
@software{microbotica,
  title = {MICROBOTICA: MICROROBOTs Iterative Simulation for Clinical Adoption},
  version = {0.1.0},
  license = {AGPL-3.0-or-later},
  url = {https://github.com/MSF/MICROROBOTICA}
}
```

See [CITATION.cff](CITATION.cff) for machine-readable citation metadata.

## License

AGPL-3.0-or-later. See [LICENSE](LICENSE).
