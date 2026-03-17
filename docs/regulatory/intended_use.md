# Intended Use — MICROBOTICA

## Platform Positioning Statement

MICROBOTICA (MICROROBOTs Iterative Simulation for Clinical Adoption) is
open-source research software. It is not a medical device as defined by
EU MDR (EU 2017/745), Article 2(1). It has no medical purpose. It is not
intended for clinical use, clinical decision-making, or patient diagnosis.
It has not been CE-marked, cleared, or approved by any regulatory body.

MICROBOTICA is a research simulator for microrobot-assisted drug delivery
simulations. It provides:

- A desktop application for interactive visualisation of microrobotics simulations
- A Python scripting console for parameter manipulation and analysis
- OpenUSD-based scene management with three-layer composition
- Pluggable compute and render backends

## Layered Responsibility Model

| Layer | Project | Responsibility |
|-------|---------|---------------|
| 1 | MADDENING | Physics framework — produces simulation data |
| 2 | MIME | Microrobotics engine — domain-specific physics |
| 3 | **MICROBOTICA** | Simulator UI — visualisation, scripting, scene management |
| 4 | Commercial Product | CE-marked SaMD — bears all EU MDR obligations |

MICROBOTICA does not produce physics data. It receives, displays, and
allows interaction with physics data produced by upstream layers.
MICROBOTICA's correctness obligation is **faithful rendering**: data
received from the physics backend must be displayed without corruption
or silent modification.

## Commercial Boundary Statement

When a downstream commercial manufacturer incorporates MICROBOTICA into
a regulated medical device, MICROBOTICA is classified as SOUP (Software
of Unknown Provenance) under IEC 62304. The device manufacturer is solely
responsible for:

- Classifying MICROBOTICA under the applicable software safety class
- Performing all required verification and validation
- Assessing MICROBOTICA's known anomalies for safety relevance in their context of use
- Implementing any required risk controls or mitigations
- Maintaining regulatory submissions and post-market surveillance

## Cybersecurity Boundary Statement

MICROBOTICA does not implement security controls for clinical deployment.
It does not provide:

- User authentication or authorisation
- Encrypted data storage or transmission
- Audit trail integrity protection
- Network security controls

When deployed in a clinical context, the downstream manufacturer must
implement all required cybersecurity controls per MDCG 2019-16.

## AGPL Licence Statement

MICROBOTICA is licensed under AGPL-3.0-or-later. Downstream commercial
manufacturers must ensure their use complies with AGPL terms. The AGPL
licence does not constitute a medical device licence or regulatory approval.
