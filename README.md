# MICROROBOTICA

MICROROBOTics Iterative simulation for Clinical Adoption.

📖 **Documentation: <https://microrobotica.org/>**
🧩 The integrated IDE in the Microrobotics Simulation Framework — sits on top of [MADDENING](https://microrobotica.org/maddening/) and [MIME](https://microrobotica.org/mime/).

## What is MICROROBOTICA?

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

## Quick Start (Docker — recommended)

```bash
docker pull ghcr.io/microrobotics-simulation-framework/microrobotica:base

docker run -it --rm \
  -v $(pwd):/workspace/microbotica \
  -w /workspace/microbotica \
  -e PXR_ROOT=/opt/usd \
  ghcr.io/microrobotics-simulation-framework/microrobotica:base \
  bash

# Inside the container:
cmake --preset linux-debug
cmake --build build/debug
cd build/debug && ctest --output-on-failure
```

See [CONTRIBUTING.md](CONTRIBUTING.md) for native setup, GUI forwarding,
and IDE integration.

## Documentation

| Document | Description |
|----------|-------------|
| [Architecture](ARCHITECTURE.md) | System architecture and module map |
| [Design Decisions](DESIGN.md) | Key design decisions and rationale |
| [Contributing](CONTRIBUTING.md) | Build setup, Docker, IDE, conventions |
| [User Guide](docs/user_guide/) | Concepts and usage |
| [Developer Guide](docs/developer_guide/) | Testing, component authoring |
| [Component Guide](docs/component_guide/) | Interface and implementation docs |
| [Validation](docs/validation/) | V&V evidence, SOUP package |
| [Regulatory](docs/regulatory/) | Intended use, EU MDR guidance |
| [CHANGELOG](CHANGELOG.md) | Version history |

## Citation

If you use MICROBOTICA in academic work, please cite:

```bibtex
@software{microbotica,
  title = {MICROBOTICA: MICROROBOTics Iterative simulation for Clinical Adoption},
  version = {0.1.0},
  license = {AGPL-3.0-or-later},
  url = {https://github.com/MSF/MICROROBOTICA}
}
```

See [CITATION.cff](CITATION.cff) for machine-readable citation metadata.

## License

AGPL-3.0-or-later. See [LICENSE](LICENSE).
