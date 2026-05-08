# MICROBOTICA Documentation Architecture Plan

## Executive Summary

**MICROBOTICA** (MICROROBOTics Iterative simulation for Clinical Adoption) is a C++17/Qt 6.6 open-source research simulator for microrobot-assisted drug delivery in cerebrospinal fluid and other confined biological geometries. It occupies Layer 3 of a four-layer open-source stack:

```
Layer 1 — MADDENING  (Python, JAX, LGPL-3.0)
    JAX-based differentiable multiphysics graph framework.
    Near-complete. JOSS/arXiv target.
    Has its own DOCUMENTATION_ARCHITECTURE.md.

Layer 2 — MIME  (Python, LGPL-3.0)
    MIcrorobotics Multiphysics Engine. Built on MADDENING.
    Not yet started. IEC 62304 Class C SaMD compliance target.

Layer 3 — MICROBOTICA  (C++17 / Qt 6.6, AGPL-3.0)  ← THIS PROJECT
    Open-source research simulator.
    Inspired by NVIDIA Omniverse; OpenUSD as primary scene format.
    Not a medical device. Research platform only.

Layer 4 — Commercial Product  (future, by a spin-out or licensee)
    CE-marked SaMD built on top of the open-source stack.
    Bears all EU MDR obligations.
    Not our responsibility, but everything we write must support it.
```

MICROBOTICA serves three audiences simultaneously: researchers building and running microrobotics simulations; developers extending the platform with new physics backends, render backends, or compute backends; and downstream commercial manufacturers who need to cite MICROBOTICA's verification record in regulatory submissions — without MICROBOTICA itself making any clinical claims.

**Regulatory context**: The developer is EU-based (Netherlands). **EU MDR (EU 2017/745)** is the primary regulatory framework. A commercial product built on top of MICROBOTICA would face EU MDR Class III classification for the most demanding plausible use case: intraoperative digital twin or RL policy validation for a device operating in the central nervous system, where erroneous output could cause irreversible neurological harm or death. The documentation architecture must be designed to support this most demanding plausible classification so that less demanding use cases are automatically covered.

Under **IEC 62304**, when any downstream commercial manufacturer incorporates MICROBOTICA into a regulated product, MICROBOTICA is classified as SOUP (Software of Unknown Provenance). For a Class III device, MICROBOTICA will almost certainly be classified as **IEC 62304 Class C SOUP**, carrying the most stringent lifecycle documentation requirements. MICROBOTICA's documentation must therefore be structured to satisfy Class C SOUP assessment requirements for any downstream commercial manufacturer.

**Relationship to MADDENING's documentation architecture**: This document inherits structural patterns, regulatory boundary language, and compliance schema conventions from MADDENING's `DOCUMENTATION_ARCHITECTURE.md`. Where a section is directly inherited, this is stated explicitly. Where a section is adapted for the C++/Qt application domain, the adaptation rationale is documented. Where a section is new (no MADDENING equivalent), the design rationale is provided from first principles. This document is self-contained — it can be read without MADDENING's document, though references are provided for deeper context.

This plan is informed by the same regulatory standards as MADDENING: **ASME V&V 40**, **FDA computational modeling guidance (2016, 2023)**, **IEC 62304:2006+AMD1:2015**, **EU MDR (EU 2017/745)**, **MDCG 2019-11**, **MDCG 2019-16**, **IMDRF SaMD N10/N12/N23**, **EN ISO 13485:2016**, **IEC 62366-1:2015** (usability engineering, new for MICROBOTICA), and **ISO 14971:2019**.

---

## Table of Contents

1. [Documentation Structure](#1-documentation-structure)
2. [Regulatory Boundary Language](#2-regulatory-boundary-language)
3. [Component Documentation Standards](#3-component-documentation-standards)
4. [V&V Documentation Hooks](#4-vv-documentation-hooks)
5. [Versioning and API Stability](#5-versioning-and-api-stability)
6. [Contributor and Extensibility Documentation](#6-contributor-and-extensibility-documentation)
7. [README and Repository-Level Documentation](#7-readme-and-repository-level-documentation)
8. [Code-Embedded Documentation and Structural Hooks](#8-code-embedded-documentation-and-structural-hooks)
   - 8.1 [ComponentMeta Schema](#81-componentmeta-schema)
   - 8.2 [Session Provenance](#82-session-provenance)
   - 8.3 [Verification Test Registration](#83-verification-test-registration)
   - 8.4 [Deprecation and Stability Machinery](#84-deprecation-and-stability-machinery)
   - 8.5 [Audit Logging Interface](#85-audit-logging-interface)
   - 8.6 [Anomaly Management System](#86-anomaly-management-system)
   - 8.7 [ISO 14971 Risk Management Hooks](#87-iso-14971-risk-management-hooks)
   - 8.8 [Usability Engineering Documentation](#88-usability-engineering-documentation)
9. [Rendering Pipeline Correctness Documentation](#9-rendering-pipeline-correctness-documentation)
10. [Python Scripting API Contract](#10-python-scripting-api-contract)
11. [Cloud Provenance Documentation](#11-cloud-provenance-documentation)
12. [IEC 62304 SOUP Compliance Package](#12-iec-62304-soup-compliance-package)
13. [IEC 62304 Software Lifecycle Documentation Mapping](#13-iec-62304-software-lifecycle-documentation-mapping)
14. [EU MDR Annex I and Annex II Alignment](#14-eu-mdr-annex-i-and-annex-ii-alignment)
15. [MDCG 2019-11: Qualification and Classification](#15-mdcg-2019-11-qualification-and-classification)
16. [Configuration Management](#16-configuration-management)
17. [QMS Compatibility](#17-qms-compatibility)
18. [Downstream Integration Guide](#18-downstream-integration-guide)

**Appendices:**

- [A: Inheritance from MADDENING Summary Table](#appendix-a-inheritance-from-maddening-summary-table)
- [B: Implementation Priority](#appendix-b-implementation-priority)
- [C: Architectural Concern Resolutions](#appendix-c-architectural-concern-resolutions)
- [D: Project Setup Checklist](#appendix-d-project-setup-checklist)

---

## 1. Documentation Structure

### Inheritance Statement

This section adapts MADDENING Section 1 for a C++/Qt application. The three-tier documentation model (API reference, component guide, executable examples) is preserved; the tooling changes from Python/Sphinx-autodoc to C++/Doxygen/Breathe.

### MICROBOTICA Documentation Structure

MICROBOTICA adopts the same three-tier documentation model as MADDENING, adapted for C++17 and Qt 6.6:

```
MICROBOTICA/
├── README.md                          # Project overview + disclaimers
├── DESIGN.md                          # Architecture decisions
├── ROADMAP.md                         # Feature roadmap
├── CHANGELOG.md                       # Structured changelog
├── CITATION.cff                       # Academic citation metadata
├── GOVERNANCE.md                      # Decision-making process
├── CONTRIBUTING.md                    # Contributor guide
├── SECURITY.md                        # Security reporting
├── LICENSE                            # AGPL-3.0-or-later
├── CMakeLists.txt                     # Root build file
├── Doxyfile                           # Doxygen configuration
│
├── docs/                              # Tier 2: Comprehensive documentation
│   ├── conf.py                        # Sphinx configuration (Breathe bridge)
│   ├── index.rst                      # Documentation root
│   │
│   ├── user_guide/                    # For simulator users
│   │   ├── installation.md
│   │   ├── quickstart.md
│   │   ├── concepts.md                # USD scenes, viewports, backends
│   │   ├── scripting.md               # Python scripting via microrobotica module
│   │   ├── tutorials/                 # Step-by-step tutorials
│   │   └── faq.md
│   │
│   ├── developer_guide/               # For platform contributors
│   │   ├── architecture.md            # System architecture overview
│   │   ├── component_authoring.md     # How to add a new component
│   │   ├── interface_contracts.md     # PhysicsProcess, RenderBackend, etc.
│   │   ├── testing_standards.md       # Test requirements
│   │   ├── documentation_standards.md # Doc requirements for contributions
│   │   └── code_style.md
│   │
│   ├── component_guide/               # Architectural component docs (Tier 2)
│   │   ├── index.md                   # Component documentation overview
│   │   ├── interfaces/                # One document per abstract interface
│   │   │   ├── physics_process.md
│   │   │   ├── render_backend.md
│   │   │   ├── render_session.md
│   │   │   ├── compute_backend.md
│   │   │   ├── result_frame.md
│   │   │   ├── physics_config.md
│   │   │   └── _template.md           # Template for new interface docs
│   │   ├── implementations/           # One document per concrete impl
│   │   │   ├── stub_physics_process.md
│   │   │   ├── stub_render_session.md
│   │   │   ├── local_compute_backend.md
│   │   │   ├── scene_manager.md
│   │   │   ├── results_applicator.md
│   │   │   ├── simulation_controller.md
│   │   │   ├── viewport_widget.md
│   │   │   ├── console_widget.md
│   │   │   └── _template.md
│   │   ├── scripting_api.md           # microrobotica Python module contract
│   │   └── cloud_compute.md           # SkyPilot/ZeroMQ/Selkies architecture
│   │
│   ├── validation/                    # V&V documentation
│   │   ├── index.md                   # V&V philosophy and scope
│   │   ├── framework_verification.md  # Framework-level verification evidence
│   │   ├── component_verification/    # Per-component verification reports
│   │   │   ├── layer_stack.md         # USD layer separation tests
│   │   │   ├── results_applicator.md  # ResultFrame → USD mapping tests
│   │   │   ├── ipc_protocol.md        # ZeroMQ parameter/result fidelity
│   │   │   ├── scripting_api.md       # microrobotica module contract tests
│   │   │   └── _template.md
│   │   ├── known_anomalies.yaml       # Machine-readable anomaly registry
│   │   ├── soup_package.md            # IEC 62304 SOUP package document
│   │   └── cou_template.md            # Context-of-use template
│   │
│   ├── regulatory/                    # Regulatory context documentation
│   │   ├── intended_use.md            # Platform positioning statement
│   │   ├── eu_mdr_guidelines.md       # EU MDR alignment guide
│   │   ├── fda_guidelines.md          # FDA alignment guide (secondary)
│   │   ├── iec62304_mapping.md        # IEC 62304 lifecycle mapping
│   │   ├── mdcg_2019_11.md            # Software qualification guidance
│   │   ├── downstream_integration.md  # Three-layer SOUP chain (from MBCA's view)
│   │   └── usability_engineering.md   # IEC 62366 UI/UX compliance (NEW)
│   │
│   ├── api_reference/                 # Tier 1: Auto-generated API docs
│   │   └── (Doxygen + Breathe output)
│   │
│   ├── releases/                      # Per-version release notes
│   │   ├── v0.1.0.md
│   │   └── _template.md
│   │
│   └── bibliography.bib              # Centralized academic references
│
├── examples/                          # Tier 3: Executable examples
│   ├── scripts/                       # Python scripting examples
│   └── scenes/                        # Example USD scenes
│
├── src/                               # Source tree
│   ├── core/                          # Abstract interfaces (no Qt/USD/Python)
│   ├── stubs/                         # Stub implementations
│   ├── scene/                         # USD scene management
│   ├── viewport/                      # Viewport widgets
│   ├── simulation/                    # SimulationController
│   └── scripting/                     # pybind11 embedded interpreter
│
└── tests/                             # Test suite
    ├── verification/                  # Verification tests (Catch2)
    │   ├── test_layer_stack.cpp
    │   ├── test_results_applicator.cpp
    │   ├── test_ipc_protocol.cpp
    │   └── ...
    └── (other test files)
```

**Rationale**: The structure mirrors MADDENING's three-tier model (API reference + component guide + examples) with two key differences: (1) `docs/component_guide/` replaces `docs/algorithm_guide/` because MICROBOTICA documents architectural components and interface contracts rather than physics algorithms; (2) `docs/regulatory/usability_engineering.md` is new because MICROBOTICA has a UI while MADDENING does not.

**Tooling**: Sphinx with MyST-Parser, Doxygen for C++ API extraction, Breathe for Sphinx↔Doxygen bridge. MICROBOTICA-specific academic references in `docs/bibliography.bib` with Pandoc `[@Key]` citation syntax. The Sphinx documentation site is a subproject of `microbotica-web` (the umbrella documentation site for the ecosystem).

---

## 2. Regulatory Boundary Language

### Inheritance Statement

This section is **directly inherited** from MADDENING Section 2, with surface substitutions (project name, licence, layer position). The "platform, not product" framing, layered responsibility model, and commercial boundary language are taken verbatim in structure. The key difference is that MICROBOTICA sits at Layer 3 — the layer most likely to be the direct dependency of a commercial product — which makes the boundary language even more critical.

### `docs/regulatory/intended_use.md`

**Platform Positioning Statement**:

> MICROBOTICA is an open-source research simulator for microrobot-assisted drug delivery in cerebrospinal fluid and other confined biological geometries. It is research software distributed under the AGPL-3.0-or-later licence.
>
> MICROBOTICA is NOT a medical device as defined by EU MDR (EU 2017/745) Article 2(1), nor is it a medical device under US FDA regulations. It does not have a medical purpose. It is not intended for direct clinical use, clinical decision-making, or patient diagnosis. It has not been CE-marked, cleared, or approved by any regulatory body.
>
> MICROBOTICA is a research platform — analogous to 3D Slicer, ParaView, or NVIDIA Omniverse for research. Under the qualification criteria of MDCG 2019-11, software without a medical purpose is not a medical device regardless of whether it is subsequently used within a medical device. MICROBOTICA performs no clinical interpretation, provides no diagnostic output, and makes no therapeutic recommendations.
>
> MICROBOTICA is designed to be a credible, auditable research platform that downstream commercial manufacturers may build upon. When incorporated into a regulated medical device by any commercial entity, MICROBOTICA is classified as SOUP (Software of Unknown Provenance) under IEC 62304:2006+AMD1:2015. The device manufacturer bears full responsibility for assessing MICROBOTICA's suitability for their specific context of use and for performing all required verification and validation activities.

**Cybersecurity Boundary Statement** (MDCG 2019-16):

> MICROBOTICA exposes a network surface through two channels: (1) ZeroMQ IPC to remote MADDENING/MIME physics backends running on SkyPilot cloud infrastructure, and (2) WebRTC streaming from remote Selkies rendering containers. In the open-source research configuration, both channels assume a trusted network environment. No authentication, authorisation, or encryption is provided by MICROBOTICA itself.
>
> When MICROBOTICA is incorporated into a regulated product, the commercial integration layer is solely responsible for: securing ZeroMQ channels (CurveZMQ or TLS), authenticating Selkies streams, validating and sanitizing all simulation parameters before they reach MADDENING/MIME via IPC, protecting the Python scripting console from injection attacks, and ensuring the cloud compute infrastructure meets the security requirements of the clinical context.
>
> The embedded Python scripting console (`microrobotica` module) accepts arbitrary user input and can modify scene state and simulation parameters. In a clinical context, the manufacturer must implement appropriate access controls and parameter validation at the scripting boundary.
>
> MICROBOTICA does not implement security controls because it is designed for trusted research environments. The vulnerability reporting process and the threat model for the open-source configuration are documented in `SECURITY.md`. When MICROBOTICA is incorporated into a regulated product, the manufacturer's cybersecurity documentation (per MDCG 2019-16) must address all network surfaces listed above.

**AGPL Licence Statement**:

> MICROBOTICA is licensed under AGPL-3.0-or-later. The AGPL requires that any user who interacts with the software over a network receives the complete corresponding source code. For a commercial product that exposes MICROBOTICA's functionality over a network (e.g., a cloud-based SaaS offering), this means the AGPL-3.0 source disclosure obligation applies to the network-accessible portions of the software. Commercial entities building on MICROBOTICA should consult legal counsel regarding AGPL compliance for their specific deployment model. MICROBOTICA's upstream dependencies (MADDENING, MIME) are LGPL-3.0, which has weaker copyleft requirements — the AGPL obligation arises only from MICROBOTICA's own code.

**Layered Responsibility Model**:

| Responsibility | Owner | Applicable Standard | Evidence |
|---|---|---|---|
| Application-layer correctness (USD management, IPC protocol, UI behaviour) | MICROBOTICA project | IEC 62304 Clause 5.6 (via SOUP assessment) | Test suite, Catch2 verification tests |
| Physics correctness | MIME/MADDENING projects | IEC 62304 Clause 5.6 | MIME/MADDENING verification suites |
| SOUP assessment (MICROBOTICA) | Downstream manufacturer | IEC 62304 Clause 5.3.3, 5.3.4 | Using MICROBOTICA's SOUP package |
| SOUP assessment (MIME, MADDENING) | Downstream manufacturer | IEC 62304 Clause 5.3.3, 5.3.4 | Using MIME/MADDENING SOUP packages (SOUP-of-SOUP chain) |
| Known anomaly evaluation | Downstream manufacturer | IEC 62304 Clause 7.1.3 | Using MICROBOTICA's known anomalies registry |
| Validation for specific COU | Device manufacturer | EU MDR Annex I Section 17.2 | Experimental comparisons for their COU |
| Risk management | Device manufacturer | ISO 14971, EU MDR Annex I Chapter I | Risk management file for their device |
| Usability engineering | Device manufacturer | IEC 62366-1:2015 | Usability engineering file |
| Clinical evidence | Device manufacturer | EU MDR Annex XIV, MEDDEV 2.7/1 rev. 4 | Clinical evaluation report; PMCF plan (Class III) |
| Regulatory submission | Device manufacturer | EU MDR Article 52+ | CE marking, Notified Body review |

### The Commercial Boundary

This section is **taken verbatim in structure** from MADDENING Section 2. The same strategic decision applies: **MADDENING, MIME, and MICROBOTICA are open-source research tools. None of them will seek CE marking. None of them are medical devices. None of them carry manufacturer liability under EU MDR.**

The regulated clinical product is built by a downstream commercial entity on top of these open-source tools. That entity takes on the QMS, the Notified Body relationship, post-market surveillance obligations, and EU MDR manufacturer liability.

MICROBOTICA's position at Layer 3 — the topmost open-source layer and the most likely direct dependency of a commercial product — means its documentation boundary language is the first thing a commercial manufacturer's regulatory team will encounter. The boundary must be unambiguous.

#### AGPL Licence and the Commercial Model

MICROBOTICA is licensed under AGPL-3.0-or-later. The AGPL differs from MADDENING's LGPL-3.0 in one significant respect: it extends the source disclosure obligation to network interaction. A commercial product that deploys MICROBOTICA as a network service must provide the complete source to users who interact with it over the network.

This does **not** prevent commercial use. It does impose a stronger copyleft requirement that commercial entities must evaluate:

- **Commercial use is permitted**: The AGPL explicitly permits commercial use.
- **Strong copyleft**: Unlike the LGPL, the AGPL's "network use" clause means that if a commercial product serves MICROBOTICA's functionality over a network, the AGPL-licensed source must be made available to network users.
- **Dual-licensing option**: The developer retains the option to offer commercial licences that waive the AGPL network disclosure requirement, enabling proprietary SaaS deployments without source disclosure.
- **Upstream compatibility**: MADDENING and MIME (LGPL-3.0) can be incorporated into AGPL-3.0 code. The AGPL is a "stronger" licence compatible with the LGPL for combination purposes.

Commercial entities should assess AGPL compatibility with their deployment model. For on-premises deployment (no network interaction), the AGPL source disclosure obligation may not be triggered. For cloud/SaaS deployment, it will be.

---

## 3. Component Documentation Standards

### Inheritance Statement

This section **adapts** MADDENING Section 3 (Algorithm and Model Documentation Standards) to the application-layer domain. MADDENING documents physics nodes with governing equations, discretization, and analytical benchmarks. MICROBOTICA documents architectural components — abstract interfaces, concrete implementations, and the `microrobotica` scripting API — with design rationale, interface contracts, failure modes, and verification evidence. The template structure is preserved; the content domain changes.

### Component Documentation Template

Every abstract interface in `src/core/` and every significant implementation class must have a corresponding document in `docs/component_guide/`. The template below draws from MADDENING's algorithm guide template, adapted for architectural components.

#### Interface Documentation Template (`docs/component_guide/interfaces/_template.md`)

```markdown
---
bibliography: ../../bibliography.bib
---

# [Interface Name]

**Header**: `src/core/[header].h`
**Stability**: [experimental | provisional | stable | deprecated]
**Component ID**: `MBCA-COMP-[XXX]`
**Version**: [semantic version of this interface]

## Summary

[1-2 sentence description of what this interface abstracts.]

## Design Rationale

[Why this interface exists. What architectural decision it enables.
What would break if it were removed or redesigned.]

## Interface Contract

[The formal contract that all implementations must satisfy.
Include pre-conditions, post-conditions, invariants, and
thread-safety guarantees.]

```cpp
// Key interface methods with Doxygen-style documentation
class PhysicsProcess {
public:
    /// @pre Configuration has been validated by the caller.
    /// @post Launches the physics backend. Subsequent calls to
    ///       receiveResult() will return ResultFrame objects.
    /// @throws std::runtime_error if launch fails.
    virtual void launch(const PhysicsConfig& config) = 0;

    /// @pre launch() has been called successfully.
    /// @post Returns the next ResultFrame from the physics backend.
    /// @note Blocking call. Returns std::nullopt on backend shutdown.
    virtual std::optional<ResultFrame> receiveResult() = 0;

    /// @pre launch() has been called successfully.
    /// @post Parameters are transmitted to the physics backend.
    ///       Effect is visible on the next receiveResult() call.
    virtual void sendParameters(const nlohmann::json& params) = 0;

    virtual ~PhysicsProcess() = default;
};
```

## Implementation Map

[Trace each interface method to its concrete implementations.
This is the C++ equivalent of MADDENING's equation-to-code
Implementation Mapping, adapted for interface-to-implementation
traceability. Mandatory for IEC 62304 Class C detailed design
traceability (Clause 5.4).]

| Interface Method | Implementation | Notes |
|---|---|---|
| `PhysicsProcess::launch()` | `StubPhysicsProcess::launch()` | Starts synthetic data generation thread |
| `PhysicsProcess::launch()` | `ZmqPhysicsProcess::launch()` (Phase 2) | Opens ZeroMQ REQ socket to MIME |
| `PhysicsProcess::receiveResult()` | `StubPhysicsProcess::receiveResult()` | Returns synthetic ResultFrame |

## Assumptions and Constraints

[Numbered list of architectural assumptions.]

1. [e.g., "All implementations are single-threaded per instance"]
2. [e.g., "PhysicsConfig is immutable after launch()"]
3. [e.g., "ResultFrame values are in SI units"]

## Known Limitations and Failure Modes

[Specific conditions where this interface or its implementations
fail or behave unexpectedly. Feeds into IEC 62304 SOUP anomaly
assessment.]

1. [e.g., "No timeout on receiveResult() — blocks indefinitely
   if the physics backend hangs"]
2. [e.g., "sendParameters() does not validate parameter names
   or ranges — invalid parameters are forwarded silently"]

## Hazard Hints

[Technical conditions that a downstream ISO 14971 risk manager
should evaluate. See Section 8.7 for the technical-vs-clinical
boundary principle — these are technical conditions only, NOT
clinical risk assessments.]

1. [e.g., "Undefined behaviour if receiveResult() is called before
   launch() — no runtime check enforced"]
2. [e.g., "If the remote physics backend is killed mid-step, the
   ZeroMQ transport may silently drop the in-flight ResultFrame"]

## Downstream Dependencies

[What SOUP components does this interface interact with?]

| SOUP Component | Interface Point | Data Flow |
|---|---|---|
| MIME (via ZeroMQ) | `sendParameters()` / `receiveResult()` | Bidirectional: config out, ResultFrame in |

## Verification Evidence

[Link to verification report and test files.]

- Verification report: [](../validation/component_verification/...)
- Test files: `tests/verification/test_*.cpp`

## References

[Cite using Pandoc-style `[@Key]` syntax.]

- [@Key] Author (Year). *Title*. — Relevance note.

## Changelog

| Version | Date | Change |
|---|---|---|
| 1.0.0 | 2026-XX-XX | Initial interface definition |
```

#### Implementation Documentation Template (`docs/component_guide/implementations/_template.md`)

Uses the same structure as the interface template but adds:

- **Implements**: which interface(s) this class implements
- **Dependencies**: Qt, USD, pybind11, or other concrete dependencies
- **Threading model**: which thread(s) the implementation runs on
- **Resource lifecycle**: what resources are acquired/released and when

### Bibliography and Citation System

**Taken verbatim from MADDENING Section 3**. MICROBOTICA maintains its own `docs/bibliography.bib` for application-layer references: CSF dynamics, microrobotics navigation, clinical microrobotics literature, OpenUSD architecture, Selkies/WebRTC architecture. Same `[@Key]` syntax, same `check_citations.py` CI script (adapted for MICROBOTICA's directory structure).

> **Planned extension — BiocompatibilityMeta**: When MIME introduces `BiocompatibilityMeta` on physics nodes (biological environment, protein mixture, temperature, pH, ISO 10993 regulatory context, ChEBI/UBERON/NCBI external identifiers), the `ResultFrame` will carry biocompatibility output fields alongside kinematic data. The component guide template will be extended to include a `BiocompatibilityContext` section for components that consume or display these fields. This extension is deferred to Phase 2 (MIME integration). The `ComponentMeta` struct's `validated_regimes` field is already capable of expressing biocompatibility parameter bounds (e.g., validated pH range, temperature envelope) — no schema change is needed for that layer.

---

## 4. V&V Documentation Hooks

### Inheritance Statement

This section **adapts** MADDENING Section 4 for application-layer V&V. MADDENING's V&V scope is physics correctness (analytical benchmarks, convergence studies). MICROBOTICA's V&V scope is application-layer correctness: interface fidelity, data integrity, and UI behaviour. The V&V boundary principle — "we verify our layer, not upstream layers" — is preserved identically.

### MICROBOTICA V&V Scope

**What MICROBOTICA's V&V provides**:
- **USD layer separation enforcement**: the base layer is never written by simulation code; the results layer is never persisted to disk; the override layer is the only layer the scripting API can write to.
- **ResultsApplicator correctness**: given a `ResultFrame`, the values that appear on USD prims match the input values exactly (no truncation, no unit conversion error, no wrong prim path).
- **IPC protocol fidelity**: parameters sent via `sendParameters()` arrive at the physics backend without corruption; `ResultFrame` values received are identical to what was sent.
- **Scripting API contract**: `microrobotica.scene().set_attribute()` writes to the override layer only; `microrobotica.sim().send_parameters()` validates parameter types before forwarding; `microrobotica.sim().current_frame()` returns a dict that faithfully mirrors the current `ResultFrame`.
- **Viewport correctness**: the viewport switches correctly between local (Hydra/Storm) and stream (Selkies/WebRTC) modes; the rendered frame corresponds to the current `ResultFrame` state.

**What MICROBOTICA's V&V does NOT provide**:
- **Physics accuracy**: whether the `ResultFrame` values are physically correct is MIME/MADDENING's domain, not MICROBOTICA's. MICROBOTICA faithfully renders and presents whatever it receives — it does not assess whether the received values are physically meaningful.
- **Clinical validation**: MICROBOTICA does not validate that any simulation output matches patient physiology. That is the commercial manufacturer's responsibility.

**The "faithful rendering" boundary**: MICROBOTICA's correctness obligation is that it faithfully renders and presents the data it receives from the physics backend. This is analogous to a medical display's obligation: the display must faithfully reproduce the image it receives, but it is not responsible for whether the image is diagnostically correct. This boundary is important for the downstream manufacturer's risk management file: they must assess MICROBOTICA's rendering fidelity separately from MIME/MADDENING's physics accuracy.

**The dependency acknowledgement**: MICROBOTICA's `ResultsApplicator` correctness test — "does the position in `ResultFrame` correctly map to a USD prim transform?" — implicitly assumes the `ResultFrame` values have semantic meaning. MICROBOTICA's V&V documentation acknowledges this dependency explicitly:

> MICROBOTICA's verification tests confirm that data received from the physics backend is correctly mapped to the USD scene. These tests use synthetic `ResultFrame` data with known values. Correctness is defined as: the value written to the USD prim attribute matches the value in the `ResultFrame` within floating-point equality. This verification does not assert that the `ResultFrame` values are physically meaningful — that assertion belongs to MIME/MADDENING's verification suite. The conjunction of MICROBOTICA's rendering fidelity verification and MIME/MADDENING's physics accuracy verification together establish that the displayed result is both faithfully rendered and physically correct. Neither verification alone is sufficient.

### Verification Boundary: Where MICROBOTICA Ends and Physics Begins

This boundary is critical and must be documented in `docs/validation/index.md`:

| Verification Domain | Owner | Evidence |
|---|---|---|
| Physics equations solve the right math | MADDENING | Analytical benchmarks, convergence studies |
| Physics engine applies correct CSF parameters | MIME | Domain-specific verification |
| Application displays physics output faithfully | MICROBOTICA | `ResultsApplicator` tests, layer separation tests |
| IPC transmits data without corruption | MICROBOTICA + MIME | End-to-end IPC fidelity tests |
| Scripting API behaves as documented | MICROBOTICA | Scripting contract tests |
| UI prevents unsafe user actions | MICROBOTICA | Usability engineering tests |
| Complete chain produces clinically valid output | Commercial manufacturer | Clinical evaluation |

---

## 5. Versioning and API Stability

### Inheritance Statement

**Taken verbatim** from MADDENING Section 5, with surface substitutions only.

### Semantic Versioning

MICROBOTICA follows strict semantic versioning (SemVer 2.0):

- **MAJOR** (X.0.0): Breaking changes to `src/core/` interfaces, `microrobotica` scripting API, or USD scene contract
- **MINOR** (0.X.0): New components, new backends, new scripting API methods (backward-compatible)
- **PATCH** (0.0.X): Bug fixes, documentation updates, performance improvements

Pre-release versions: `X.Y.Z-alpha.N`, `X.Y.Z-beta.N`, `X.Y.Z-rc.N`

### `CHANGELOG.md`

Following [Keep a Changelog](https://keepachangelog.com/) with regulatory-specific sections:

```markdown
# Changelog

All notable changes to MICROBOTICA are documented in this file.

## [Unreleased]

### Added
### Changed
### Deprecated
### Removed
### Fixed
### Verification
- [Tracks changes to V&V status]
### Security
- [Required by MDCG 2019-16 cybersecurity guidance]
### Known Anomalies
- [Changes to known_anomalies.yaml — required for IEC 62304 SOUP]
```

### Commit Message Convention

| Prefix | Meaning |
|---|---|
| `feat:` | New feature |
| `fix:` | Bug fix |
| `refactor:` | Code restructuring (no behaviour change) |
| `docs:` | Documentation only |
| `test:` | Test additions or changes |
| `perf:` | Performance improvement |
| `verify:` | Verification/validation evidence |
| `break:` | Breaking change |
| `deprecate:` | Deprecation notice |
| `security:` | Security-relevant change |
| `ui:` | UI/UX change (new — MICROBOTICA-specific) |

### API Stability Levels

Each public API surface carries a stability level (see Section 8.4 for code-level machinery):

| Level | Meaning | SemVer Guarantee |
|---|---|---|
| **stable** | Covered by SemVer; breaking changes only in major versions | Full |
| **provisional** | API may change in minor versions with deprecation warnings | One minor version notice |
| **experimental** | API may change without notice | None |
| **deprecated** | Scheduled for removal; use alternative | Removed in next major |

---

## 6. Contributor and Extensibility Documentation

### Inheritance Statement

This section **adapts** MADDENING Section 7 for a C++/Qt project. The contributor checklist structure is preserved; the checklist items change to reflect C++ artifacts.

### `CONTRIBUTING.md` (Repository Root)

Top-level contributor guide covering: development environment setup (Qt 6.6, OpenUSD build, pybind11), CMake build instructions, branching model, commit convention, code style, and links to detailed guides.

### `docs/developer_guide/component_authoring.md`

Guide for adding a new component to MICROBOTICA:

1. **The interface contract**: pure virtual classes in `src/core/`, no Qt/USD/Python dependencies
2. **Directory structure**: where the files live, naming conventions
3. **Required documentation**: Doxygen comments + component guide document
4. **Required tests**: Catch2 unit tests + at least one verification test
5. **Required metadata**: `ComponentMeta` struct (Section 8.1)
6. **Checklist**:

```markdown
### New Component Checklist

- [ ] Abstract interface in `src/core/` (if new interface)
- [ ] Concrete implementation in appropriate `src/` subdirectory
- [ ] `ComponentMeta` attached to the implementation (component ID, stability, hazard hints)
- [ ] Doxygen documentation on all public methods
- [ ] Component guide document in `docs/component_guide/`
- [ ] Design rationale documented
- [ ] Interface contract (pre/post conditions) documented
- [ ] Assumptions and constraints listed
- [ ] Known limitations and failure modes documented
- [ ] Hazard hints for ISO 14971 hazard identification
- [ ] At least one Catch2 verification test
- [ ] Entry in `docs/bibliography.bib` for primary reference (if applicable)
- [ ] Known limitations entered in `docs/validation/known_anomalies.yaml`
- [ ] CI passes on all platforms
```

### `docs/developer_guide/testing_standards.md`

Test requirements:

1. **Unit tests** (mandatory): test each public method using Catch2
2. **Integration tests** (mandatory for implementations): test the implementation through its abstract interface
3. **Verification tests** (mandatory for `src/core/` interfaces and safety-relevant implementations): registered via the verification test pattern (Section 8.3); test data integrity, interface contract enforcement, and failure mode behaviour

**4. Memory Safety Analysis (mandatory for all `src/` code)**

MICROBOTICA requires memory safety analysis as part of CI. The following tools are used:

**AddressSanitizer + UndefinedBehaviorSanitizer (ASan/UBSan)**

Enabled via a dedicated CMake preset. This is the primary memory safety CI job and must pass on every pull request.

```cmake
# CMakePresets.json — add alongside linux-debug and linux-release
{
    "name": "linux-asan",
    "inherits": "linux-debug",
    "binaryDir": "${sourceDir}/build/asan",
    "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_CXX_FLAGS": "-fsanitize=address,undefined -fno-omit-frame-pointer -g",
        "CMAKE_EXE_LINKER_FLAGS": "-fsanitize=address,undefined"
    }
}
```

Catches: heap use-after-free, stack buffer overflow, uninitialised memory reads, null pointer dereference, signed integer overflow, invalid enum values, out-of-bounds array access. LeakSanitizer (LSan) is enabled automatically with ASan on Linux and catches memory leaks at process exit.

**Known false-positive scope**: pybind11 intentionally retains some Python interpreter state at shutdown (CPython limitation — not all extension modules can fully unload). LSan will report these as leaks. Suppress them with a `lsan.suppressions` file:

```
# lsan.suppressions — suppress known pybind11/CPython shutdown leaks
leak:libpython
leak:pybind11
leak:Py_Finalize
```

Pass to the test runner with `LSAN_OPTIONS=suppressions=lsan.suppressions`.

**ThreadSanitizer (TSan)**

Enabled via a separate CMake preset. Run as a nightly CI job or on branches that touch `src/scripting/` or any code that crosses the Qt main thread / Python thread boundary.

```cmake
{
    "name": "linux-tsan",
    "inherits": "linux-debug",
    "binaryDir": "${sourceDir}/build/tsan",
    "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_CXX_FLAGS": "-fsanitize=thread -fno-omit-frame-pointer -g",
        "CMAKE_EXE_LINKER_FLAGS": "-fsanitize=thread"
    }
}
```

**Important**: ASan and TSan are mutually exclusive — they cannot run in the same binary. CI uses two separate jobs: `asan-ubsan` (PRs) and `tsan` (nightly or scripting-related PRs).

Catches: data races between threads. The primary concern in MICROBOTICA is the `microrobotica` scripting module: Python scripts execute on a Python thread, but all `microrobotica` API calls dispatch to the Qt main thread via `QMetaObject::invokeMethod(..., Qt::BlockingQueuedConnection)`. This pattern is correct and race-free by design, but TSan may flag Qt internal dispatch without a suppression. Add:

```
# tsan.suppressions — suppress known Qt/Python thread dispatch patterns
race:QMetaObject::invokeMethod
race:QCoreApplication::postEvent
```

**Valgrind/Memcheck**

Run manually or as a weekly CI job on the Catch2 verification test suite. Slower than ASan but catches different classes of errors (particularly conditional jumps on uninitialised values that ASan misses). Not required to pass on every PR due to runtime cost, but any Valgrind error in `tests/verification/` is treated as a blocking issue.

```bash
valgrind --tool=memcheck \
         --leak-check=full \
         --show-leak-kinds=definite,indirect \
         --track-origins=yes \
         --suppressions=valgrind.suppressions \
         ./build/debug/tests/microbotica_tests \
         "[verification]"
```

Maintain a `valgrind.suppressions` file for known false positives (Qt internals, OpenUSD static initialisation). Only suppress with a comment explaining why the suppression is safe.

**CI scope — test binary, not the full application**:

The ASan/UBSan and TSan jobs compile and run the **Catch2 test binary only** — not the full Qt application. The test binary links `src/core/`, `src/scene/`, `src/simulation/`, and `src/scripting/` directly, without instantiating a `QApplication` or invoking OpenUSD's rendering stack. This is deliberate: the code paths where memory errors are safety-relevant (`ResultsApplicator`, `SceneManager`, `SimulationController`, `ScriptingEngine`) are fully exercised by the headless test suite. The sanitizer-instrumented test binary builds and runs in 2-3 minutes on a standard GitHub Actions runner.

The full application binary (Qt + OpenUSD + Hydra/Storm) is **not** rebuilt on every PR. OpenUSD alone takes 20-40 minutes to build from source. The correct approach is to keep the USD and Qt builds in a cached Docker image that is rebuilt manually only when the version pin changes, and pulled in CI as a layer. NVIDIA publishes prebuilt OpenUSD binaries for Ubuntu that are suitable for this purpose. MICROBOTICA's own `src/` compiles against the cached USD headers in under a minute.

**Consequence**: the `asan-ubsan` CI job must be structured as:
1. Pull the cached USD/Qt Docker image (or restore from GitHub Actions cache using the USD version as the cache key)
2. Build only `microbotica_tests` target with ASan flags — do not build the `microbotica` application target
3. Run `./microbotica_tests "[verification]"` and `./microbotica_tests "[unit]"` with `LSAN_OPTIONS=suppressions=lsan.suppressions`

If the USD version pin changes, update the Docker image or cache key — do not attempt to build USD from source in the sanitizer job.

**CI job structure**:

| Job | Trigger | Target built | Tools | Duration |
|---|---|---|---|---|
| `build-test` | Every PR | `microbotica_tests` | GCC, no sanitizers | ~2 min |
| `asan-ubsan` | Every PR | `microbotica_tests` | ASan + UBSan + LSan | ~3 min |
| `tsan` | Nightly + scripting PRs | `microbotica_tests` | TSan | ~3 min |
| `valgrind` | Weekly | `microbotica_tests` | Valgrind/Memcheck | ~15 min |
| `full-build` | Nightly | `microbotica` (full app) | GCC, no sanitizers | ~5 min with cache |

All jobs assume the USD/Qt Docker image is pre-cached. The `full-build` job is a smoke test only — it verifies the application links correctly but does not run the GUI.

**Regulatory note**: Memory safety analysis results are part of the IEC 62304 Clause 5.6 verification evidence. Any memory error found by these tools that is not immediately fixed must be entered into `docs/validation/known_anomalies.yaml` as an `MBCA-ANO-*` entry with `safety_relevance` assessed. A memory error that affects `ResultsApplicator`, `SceneManager`, or `SimulationController` is presumptively `safety_relevant: "context_dependent"` until assessed otherwise.

---

## 7. README and Repository-Level Documentation

### Inheritance Statement

**Taken verbatim in structure** from MADDENING Section 8, adapted for MICROBOTICA.

### README.md

```markdown
# MICROBOTICA

MICROROBOTics Iterative simulation for Clinical Adoption.

[Badges: CI status, test count, license]

## What is MICROBOTICA?

MICROBOTICA is an open-source research simulator for microrobot-assisted
drug delivery in cerebrospinal fluid and other confined biological
geometries. Built on C++17/Qt 6.6 with OpenUSD as the primary scene
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

[Installation, minimal example]

## Documentation

| Document | Description |
|----------|-------------|
| [User Guide](docs/user_guide/) | Installation, tutorials, concepts |
| [Component Guide](docs/component_guide/) | Architectural component docs |
| [Developer Guide](docs/developer_guide/) | Contributing, testing, code style |
| [Validation](docs/validation/) | V&V evidence, SOUP package |
| [Regulatory](docs/regulatory/) | Intended use, EU MDR/FDA guidance |
| [API Reference](docs/api_reference/) | Auto-generated C++ API docs |
| [DESIGN.md](DESIGN.md) | Architecture decisions |
| [ROADMAP.md](ROADMAP.md) | Feature roadmap |
| [CHANGELOG.md](CHANGELOG.md) | Version history |

## Citation

If you use MICROBOTICA in academic work, please cite:

[BibTeX entry]

## License

AGPL-3.0-or-later. See [LICENSE](LICENSE).
```

### Root-Level Governance Files

| File | Purpose |
|---|---|
| `CITATION.cff` | Machine-readable citation metadata + configuration management artifact |
| `GOVERNANCE.md` | Decision-making process, maintainer roles |
| `CONTRIBUTING.md` | Quick-start for contributors |
| `SECURITY.md` | Vulnerability reporting (supports MDCG 2019-16 cybersecurity) |
| `CODE_OF_CONDUCT.md` | Community standards |

---

## 8. Code-Embedded Documentation and Structural Hooks

This section proposes concrete code-level mechanisms for MICROBOTICA. It adapts MADDENING Section 9 for a C++17 codebase.

### 8.1 ComponentMeta Schema

#### Inheritance Statement

This section **adapts** MADDENING Section 9.1 (`NodeMeta`) for C++17. The key architectural decision is how to attach structured metadata to C++ abstract interfaces without pulling in heavy dependencies.

#### Design Decision: `ComponentMeta` as a Plain C++ Struct in `src/core/`

**Resolution of Concern 1 (ComponentMeta scope in C++)**:

`ComponentMeta` is a plain C++ struct defined in `src/core/component_meta.h`. This location ensures:

1. **Dependency-free**: `src/core/` has no Qt, USD, or Python dependencies. `ComponentMeta` uses only standard library types (`std::string`, `std::vector`, `std::optional`), nlohmann_json for serialization, and nothing else.
2. **Embeddable in headers**: Any `src/core/` interface header can `#include "component_meta.h"` without pulling in transitive dependencies.
3. **Machine-readable**: The struct serializes to JSON via nlohmann_json, enabling automated harvesting for SOUP package generation, capability matrices, and CI validation scripts.

**Granularity**: One `ComponentMeta` instance per abstract interface AND per concrete implementation that adds new failure modes or hazard hints. The abstract interface's `ComponentMeta` documents the contract-level properties (what all implementations must satisfy). Each concrete implementation's `ComponentMeta` documents implementation-specific properties (transport-specific failure modes, platform-specific limitations). See Section 8.7 for how this granularity interacts with hazard hint scoping.

#### Design

```cpp
// src/core/component_meta.h

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace microbotica::core {

/// Stability level for API surfaces and components.
enum class StabilityLevel {
    Experimental,
    Provisional,
    Stable,
    Deprecated
};

NLOHMANN_JSON_SERIALIZE_ENUM(StabilityLevel, {
    {StabilityLevel::Experimental, "experimental"},
    {StabilityLevel::Provisional, "provisional"},
    {StabilityLevel::Stable, "stable"},
    {StabilityLevel::Deprecated, "deprecated"},
})

/// A quantitative bound on a validated parameter range.
struct ValidatedRegime {
    std::string parameter;
    double min_value;
    double max_value;
    std::string units;
    std::string evidence;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ValidatedRegime,
        parameter, min_value, max_value, units, evidence)
};

/// A citable academic or technical reference.
struct Reference {
    std::string key;          ///< BibTeX key in bibliography.bib
    std::string description;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Reference, key, description)
};

/// Structured metadata for a MICROBOTICA component.
///
/// Machine-readable and automatically harvestable for V&V reports,
/// SOUP package documents, and IEC 62304 SOUP assessment.
///
/// The ComponentMeta struct is the C++ equivalent of MADDENING's
/// NodeMeta Python dataclass. It lives in src/core/ to ensure
/// zero Qt/USD/Python dependency. Serializes to JSON via
/// nlohmann_json for CI script consumption.
struct ComponentMeta {
    std::string component_id;           ///< e.g., "MBCA-COMP-001"
    std::string component_version;      ///< e.g., "1.0.0"
    StabilityLevel stability = StabilityLevel::Experimental;
    std::string description;

    // Interface contract
    std::vector<std::string> preconditions;
    std::vector<std::string> postconditions;
    std::vector<std::string> invariants;

    // Design rationale
    std::string design_rationale;

    // Assumptions and constraints
    std::vector<std::string> assumptions;
    std::vector<std::string> limitations;

    // Validated regimes (quantitative, parameter-bound)
    std::vector<ValidatedRegime> validated_regimes;

    // Hazard hints (qualitative, non-parameter-bound)
    // See Section 8.7 for scope distinction.
    std::vector<std::string> hazard_hints;

    // References
    std::vector<Reference> references;

    // Deprecation
    std::optional<std::string> deprecation_notice;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ComponentMeta,
        component_id, component_version, stability, description,
        preconditions, postconditions, invariants,
        design_rationale, assumptions, limitations,
        validated_regimes, hazard_hints, references,
        deprecation_notice)
};

// std::optional serialization requires nlohmann_json >= 3.11.0
// Ensure vcpkg.json pins: "nlohmann-json": ">=3.11.0"
// If building without vcpkg, verify: nlohmann/json.hpp version >= 3.11.0
static_assert(NLOHMANN_JSON_VERSION_MAJOR > 3 ||
              (NLOHMANN_JSON_VERSION_MAJOR == 3 && NLOHMANN_JSON_VERSION_MINOR >= 11),
              "ComponentMeta requires nlohmann_json >= 3.11.0 for std::optional support");

} // namespace microbotica::core
```

> **Dependency note**: `component_meta.h` depends on `<nlohmann/json.hpp>`. This is the only third-party dependency in `src/core/`. The tradeoff is deliberate: machine-readable JSON serialization is essential for CI harvest scripts (`scripts/collect_component_meta.py`), SOUP package generation, and capability matrix generation. nlohmann_json is header-only, adds no link-time dependency, and is already a vcpkg dependency of the main executable. If a future embedded or constrained context requires a truly dependency-free `src/core/`, the serialization methods can be removed or conditionally compiled via `#ifdef MBCA_ENABLE_JSON_SERIALIZATION`.
>
> **Version requirement**: nlohmann_json >= 3.11.0 is required for `std::optional` serialization. Pin this in `vcpkg.json` with `"nlohmann-json": ">=3.11.0"`. Earlier versions will compile but silently omit `deprecation_notice` from JSON output.

#### Integration with Abstract Interfaces

Each abstract interface in `src/core/` exposes metadata via a static method:

```cpp
// src/core/physics_process.h

#include "component_meta.h"

class PhysicsProcess {
public:
    /// Return structured metadata for this interface.
    /// Concrete implementations override with their own metadata.
    static const ComponentMeta& interfaceMeta() {
        static const ComponentMeta meta{
            .component_id = "MBCA-COMP-001",
            .component_version = "1.0.0",
            .stability = StabilityLevel::Experimental,
            .description = "Abstract interface for physics computation backends",
            .preconditions = {"PhysicsConfig validated by caller before launch()"},
            .postconditions = {"After launch(), receiveResult() returns valid frames"},
            .assumptions = {
                "Single PhysicsProcess instance per SimulationController",
                "ResultFrame values are in SI units",
                "PhysicsConfig is immutable after launch()",
            },
            .limitations = {
                "No timeout on receiveResult() — blocks indefinitely if backend hangs",
                "sendParameters() does not validate parameter names or ranges",
            },
            .hazard_hints = {
                "Undefined behaviour if receiveResult() is called before launch()",
                "No mechanism to detect if the physics backend has entered a "
                "numerically unstable state — MICROBOTICA will faithfully render "
                "divergent results without warning unless the upstream physics "
                "engine provides health status in the ResultFrame",
            },
        };
        return meta;
    }

    // ... interface methods ...
};
```

Concrete implementations provide their own `meta()`:

```cpp
// src/stubs/stub_physics_process.h

class StubPhysicsProcess : public PhysicsProcess {
public:
    static const ComponentMeta& meta() {
        static const ComponentMeta meta{
            .component_id = "MBCA-IMPL-001",
            .component_version = "1.0.0",
            .stability = StabilityLevel::Experimental,
            .description = "Stub physics backend generating synthetic data for architecture proving",
            .assumptions = {
                "Synthetic data uses hardcoded sinusoidal trajectories",
                "No physical meaning — for architecture proving only",
                "simTime increases monotonically at 60 Hz regardless of wall time",
            },
            .limitations = {
                "Fixed output format — does not respond to sendParameters()",
                "Produces only position and simTime fields in ResultFrame",
                "Does not simulate any physical phenomenon",
            },
            .hazard_hints = {
                "Output is synthetic, not physical — must never be used for "
                "any validation or clinical purpose",
                "Does not exercise the ZeroMQ transport path — IPC-related "
                "failure modes (frame drops, reordering, latency) are not "
                "testable with this stub",
            },
        };
        return meta;
    }
    // ...
};
```

#### Automatic Harvesting

A CI script (`scripts/harvest_component_meta.py`) discovers all `ComponentMeta` instances by parsing C++ headers for the `ComponentMeta` struct initializations, validates that all required fields are populated, and generates a JSON capability matrix. This is the C++ equivalent of MADDENING's `collect_node_metadata()`.

#### Scope Distinction: `validated_regimes` vs. `hazard_hints`

Identical to MADDENING Section 9.1:

| Field | Nature | Scope | Example | Consumer |
|---|---|---|---|---|
| `validated_regimes` | **Quantitative, parameter-bound** | Defines the envelope within which the component has been verified | `ValidatedRegime("max_prims_in_scene", 1, 10000, "", "Performance test")` | Runtime validators, component guide tables |
| `hazard_hints` | **Qualitative, non-parameter-bound** | Technical conditions that cannot be reduced to a single parameter range | `"If the physics backend is killed mid-step, in-flight ResultFrame is silently dropped"` | ISO 14971 hazard identification, SOUP package |

---

### 8.2 Session Provenance

#### Inheritance Statement

**Adapts** MADDENING Section 9.2 for a desktop application with cloud compute backends.

#### Design

```cpp
// src/core/session_provenance.h

#pragma once

#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace microbotica::core {

/// Complete, serializable record of a MICROBOTICA session.
///
/// Captures everything needed to reproduce the session and to serve
/// as an audit trail in a regulatory context. Extends MADDENING's
/// SimulationProvenance with application-layer and cloud provenance.
struct SessionProvenance {
    // Software identity
    std::string microbotica_version;    ///< IEC 62304 Clause 8: SOUP identification
    std::string qt_version;
    std::string usd_version;
    std::string compiler_version;
    std::string platform;

    // Upstream SOUP versions (captured from IPC handshake)
    std::optional<std::string> mime_version;
    std::optional<std::string> maddening_version;

    // Scene identity
    std::string scene_file_path;
    std::string scene_file_hash;        ///< SHA-256 of the loaded USD file

    // Compute backend
    std::string compute_backend_type;   ///< "local" | "skypilot"
    std::optional<std::string> skypilot_job_id;
    std::optional<std::string> cloud_provider;
    std::optional<std::string> container_image_hash;

    // Render backend
    std::string render_backend_type;    ///< "local_hydra" | "selkies_stream"
    std::optional<std::string> selkies_session_id;

    // Simulation parameters
    nlohmann::json initial_parameters;
    std::vector<nlohmann::json> parameter_changes;  ///< Timestamped changes

    // Timestamps
    std::string session_started;
    std::string session_ended;

    // Results summary
    int total_frames_received = 0;
    int frames_dropped = 0;
    int out_of_range_warnings = 0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(SessionProvenance,
        microbotica_version, qt_version, usd_version,
        compiler_version, platform,
        mime_version, maddening_version,
        scene_file_path, scene_file_hash,
        compute_backend_type, skypilot_job_id, cloud_provider,
        container_image_hash,
        render_backend_type, selkies_session_id,
        initial_parameters, parameter_changes,
        session_started, session_ended,
        total_frames_received, frames_dropped, out_of_range_warnings)
};

// std::optional serialization requires nlohmann_json >= 3.11.0
// Ensure vcpkg.json pins: "nlohmann-json": ">=3.11.0"
static_assert(NLOHMANN_JSON_VERSION_MAJOR > 3 ||
              (NLOHMANN_JSON_VERSION_MAJOR == 3 && NLOHMANN_JSON_VERSION_MINOR >= 11),
              "SessionProvenance requires nlohmann_json >= 3.11.0 for std::optional support");

} // namespace microbotica::core
```

> **Version requirement**: Same nlohmann_json >= 3.11.0 requirement as `component_meta.h` — required for `std::optional` serialization of `mime_version`, `maddening_version`, and cloud provenance fields. The `static_assert` above will produce a clear compile-time error if an older version is used. `audit_logger.h` (Section 8.5) carries the same requirement when implemented in Phase 2.

---

### 8.3 Verification Test Registration

#### Inheritance Statement

**Adapts** MADDENING Section 9.3 for Catch2 tests. The decorator pattern becomes a Catch2 tag + a registry header.

#### Design

```cpp
// src/core/verification_registry.h

#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace microbotica::core {

enum class BenchmarkType {
    DataIntegrity,      ///< Data passes through without corruption
    ContractEnforcement,///< Interface pre/post conditions hold
    LayerSeparation,    ///< USD layer invariants maintained
    ProtocolFidelity,   ///< IPC data fidelity
    UIBehavior,         ///< UI responds correctly to state changes
    Regression          ///< Regression test against known output
};

struct VerificationBenchmark {
    std::string benchmark_id;       ///< e.g., "MBCA-VER-001"
    std::string component_id;       ///< e.g., "MBCA-COMP-003"
    BenchmarkType type;
    std::string description;
    std::string test_file;          ///< e.g., "tests/verification/test_layer_stack.cpp"
    std::string test_case;          ///< Catch2 test case name
};

/// Global verification benchmark registry.
/// Populated at static init time via REGISTER_VERIFICATION_BENCHMARK.
std::vector<VerificationBenchmark>& benchmarkRegistry();

} // namespace microbotica::core

/// Register a Catch2 test as a verification benchmark.
///
/// Parameters:
///   token     — a valid C++ identifier token (no quotes, no dashes),
///               e.g. MBCA_VER_001. Used only for ODR-safe static init.
///   id_str    — the human-readable benchmark ID string, e.g. "MBCA-VER-001"
///   comp_id   — the component ID string, e.g. "MBCA-COMP-010"
///   type      — BenchmarkType enum value
///   desc      — description string
///   file      — source file path (use __FILE__)
///   test_name — Catch2 TEST_CASE name string
///
/// Usage: place in a .cpp file alongside the Catch2 TEST_CASE.
#define REGISTER_VERIFICATION_BENCHMARK(token, id_str, comp_id, type, desc, file, test_name) \
    namespace { \
        static const bool _reg_##token = []() { \
            microbotica::core::benchmarkRegistry().push_back({ \
                id_str, comp_id, type, desc, file, test_name}); \
            return true; \
        }(); \
    }
```

#### Usage

```cpp
// tests/verification/test_layer_stack.cpp

#include <catch2/catch_test_macros.hpp>
#include "core/verification_registry.h"

REGISTER_VERIFICATION_BENCHMARK(
    MBCA_VER_001,                          // token: valid C++ identifier
    "MBCA-VER-001",                        // id_str: human-readable ID
    "MBCA-COMP-010",                       // component ID
    microbotica::core::BenchmarkType::LayerSeparation,
    "Base layer is never written by ResultsApplicator",
    __FILE__,
    "ResultsApplicator never writes to base layer"
)

TEST_CASE("ResultsApplicator never writes to base layer",
          "[verification][layer-separation]") {
    // ... test implementation ...
}
```

> **Token convention**: The `token` parameter must be a valid C++ identifier — use underscores instead of dashes, no quotes. The conventional mapping is: `MBCA-VER-001` → `MBCA_VER_001`, `MBCA-VER-002` → `MBCA_VER_002`, etc. The `id_str` string is what appears in generated JSON reports and the verification traceability table; the `token` is only used to guarantee ODR-safe static initialization.

CI runs all `[verification]`-tagged Catch2 tests and generates structured JSON reports for `docs/validation/`.

#### Phase 0 Verification Benchmarks

The following verification benchmarks are defined for Phase 0. These are architectural decisions — the test implementations will be written as part of Phase 0/1 development.

| Benchmark ID | Component ID | Type | Description |
|---|---|---|---|
| `MBCA-VER-001` | `MBCA-COMP-010` | LayerSeparation | Base layer is never written by ResultsApplicator |
| `MBCA-VER-002` | `MBCA-COMP-010` | LayerSeparation | Results layer is never persisted to disk |
| `MBCA-VER-003` | `MBCA-COMP-011` | DataIntegrity | ResultFrame position values map exactly to USD `xformOp:translate` |
| `MBCA-VER-004` | `MBCA-COMP-011` | DataIntegrity | ResultsApplicator logs a warning for unknown prim paths (not silent) |
| `MBCA-VER-005` | `MBCA-COMP-040` | UIBehavior | `microrobotica.scene().set_attribute()` writes to override layer only |
| `MBCA-VER-006` | `MBCA-COMP-001` | Regression | StubPhysicsProcess produces monotonically increasing simTime |
| `MBCA-VER-007` | `MBCA-COMP-011` | DataIntegrity | Orientation values map exactly to USD `xformOp:orient` |
| `MBCA-VER-008` | `MBCA-COMP-011` | DataIntegrity | MeshData vertexColors map exactly to `primvars:displayColor` |
| `MBCA-VER-009` | `MBCA-IMPL-010` | ProtocolFidelity | ResultFrame JSON survives ZMQ PUB/SUB transport without corruption |
| `MBCA-VER-010` | `MBCA-IMPL-012` | DataIntegrity | MimeStubPhysicsProcess data arrives at USD prims via demo scene pipeline |

**Note on MBCA-VER-004**: This benchmark documents the *desired* behaviour that fixes `MBCA-ANO-001` — the verification test should currently fail until the fix is implemented. This is intentional: verification tests are written against the correct behaviour, not the current behaviour. A failing MBCA-VER-004 in CI is expected until the ResultsApplicator logging fix lands. This benchmark is the `resolution_verification` target for `MBCA-ANO-001`. When `MBCA-VER-004` passes in CI, `MBCA-ANO-001` may be updated to `resolution_status: "fixed"` with `resolution_verification: "MBCA-VER-004"`.

---

### 8.4 Deprecation and Stability Machinery

#### Inheritance Statement

**Adapts** MADDENING Section 9.5 for C++17. Uses `[[deprecated]]` attribute and a compile-time registry.

#### Design

```cpp
// src/core/stability.h

#pragma once

#include <spdlog/spdlog.h>

/// Mark a class or function as deprecated with a compiler warning.
#define MBCA_DEPRECATED(msg) [[deprecated(msg)]]

/// Mark a class as experimental.
/// Emits a runtime spdlog::warn on the first instantiation of the class,
/// not at program startup. Place in the class constructor body.
///
/// Usage:
///   MyExperimentalClass::MyExperimentalClass() {
///       MBCA_EXPERIMENTAL_WARN("MyExperimentalClass");
///       // ... rest of constructor
///   }
#define MBCA_EXPERIMENTAL_WARN(class_name) \
    do { \
        static bool _warned = false; \
        if (!_warned) { \
            spdlog::warn("{} is experimental — API may change without notice. " \
                         "Do not rely on this interface in production.", class_name); \
            _warned = true; \
        } \
    } while(0)
```

**Thread-safety note**: The `static bool _warned` inside `MBCA_EXPERIMENTAL_WARN` is not thread-safe. For Phase 0 (single-threaded construction) this is acceptable. If construction is ever moved to worker threads, replace with `std::atomic<bool>`.

---

### 8.5 Audit Logging Interface

#### Inheritance Statement

**Adapts** MADDENING Section 9.6 for C++17. Uses spdlog as the logging backend with a dedicated audit log sink.

#### Design

```cpp
// src/core/audit_logger.h

#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace microbotica::core {

/// Lightweight audit logger for regulatory audit trails.
///
/// Writes structured JSON events to a session-specific audit log.
/// Activated when audit_enabled=true in application config.
/// Uses NullSink by default (zero overhead).
///
/// Captured events:
/// - Session start/end (with SessionProvenance)
/// - Scene load (path, hash)
/// - Compute backend selection (local/cloud)
/// - Parameter changes (with timestamp, old/new values)
/// - ResultFrame receipt (frame number, timestamp)
/// - Out-of-range warnings (which values, what thresholds)
/// - Scripting console commands (input, success/failure)
/// - Cloud provenance (SkyPilot job ID, container hash, RTT)
class AuditLogger {
public:
    /// Start with no-op sink. Call enable() to activate.
    AuditLogger() = default;

    /// Enable audit logging to a file.
    void enable(const std::string& log_path);

    /// Log a structured event.
    void logEvent(const std::string& event_type,
                  const nlohmann::json& data);

    /// Log a session start with full provenance.
    void logSessionStart(const SessionProvenance& provenance);

    /// Log a parameter change with old and new values.
    void logParameterChange(const std::string& param_name,
                            const nlohmann::json& old_value,
                            const nlohmann::json& new_value);

    /// Log receipt of a ResultFrame.
    void logResultFrame(int frame_number, double sim_time,
                        bool any_out_of_range);

    /// Log a scripting console command.
    void logScriptCommand(const std::string& command,
                          bool success,
                          const std::string& error_msg = "");

private:
    std::shared_ptr<spdlog::logger> logger_;
    bool enabled_ = false;
};

} // namespace microbotica::core
```

---

### 8.6 Anomaly Management System

#### Inheritance Statement

**Taken verbatim** from MADDENING Section 9.7 with surface substitutions (prefix `MBCA-ANO-*`, project name, version field name). The three-tier release gate, YAML schema, GitHub Issues label taxonomy, and `check_anomalies.py` CI validation script are identical in structure.

#### Anomaly Record Schema

Same schema as MADDENING, with prefix `MBCA-ANO-*`:

```yaml
# MICROBOTICA Known Anomalies Registry
# Updated with each release per IEC 62304 SOUP anomaly assessment requirements
# Schema version: 1.0

schema_version: "1.0"
microbotica_version: "0.1.0"
generated_date: "2026-XX-XX"

anomalies:
  - anomaly_id: "MBCA-ANO-001"
    title: "ResultsApplicator silently drops ResultFrame fields with unknown prim paths"
    description: >
      When a ResultFrame contains a prim path that does not exist in the
      current USD stage, ResultsApplicator silently ignores it. No warning
      is logged and no error is raised. This means if the physics backend
      adds a new output field that the scene does not expect, the data is
      silently lost.
    affected_components: ["ResultsApplicator"]
    affected_versions: "0.1.0 – current"
    severity: "major"
    safety_relevance: "context_dependent"
    safety_relevance_rationale: >
      In a research context, silently dropped fields are a minor annoyance.
      In a clinical context, a dropped field could mean that a critical
      simulation output (e.g., drug concentration at a target site) is not
      displayed to the operator, who may make decisions based on incomplete
      information. The downstream manufacturer must assess whether any
      ResultFrame fields in their context of use are safety-critical.
    workaround: "Ensure scene prims match expected ResultFrame fields before simulation"
    resolution_status: "open"
    resolution_verification: "MBCA-VER-004"  # warning test that fixes the silent drop
    detected_by: "manual_review"
    memory_safety_relevant: false
    github_issue: null
    date_reported: "2026-XX-XX"

  - anomaly_id: "MBCA-ANO-002"
    title: "ScriptingEngine::execute() blocks the Qt main thread for script duration"
    description: >
      When a Python script is executed via the scripting console, the
      ScriptingEngine::execute() call runs on the Qt main thread and
      blocks the event loop for the entire duration of the script. The
      UI is completely unresponsive during execution — no viewport
      updates, no button clicks, no menu interactions are processed.
    affected_components: ["ConsoleWidget", "MicrobotaModule"]
    affected_versions: "0.1.0 – current"
    severity: "minor"
    safety_relevance: "context_dependent"
    safety_relevance_rationale: >
      In a research context, brief UI freezes during script execution
      are a minor inconvenience. In a clinical context where an operator
      needs to interrupt a running simulation or respond to an emergency,
      a blocking script could delay a safety-critical user action (e.g.,
      halting a simulation that has entered a dangerous regime). The
      downstream manufacturer must assess whether uninterruptible script
      execution is acceptable in their context of use.
    workaround: "Keep scripts short; avoid long-running computations in the console"
    resolution_status: "open"
    resolution_verification: null            # async scripting is a future architectural change
    detected_by: "manual_review"
    memory_safety_relevant: false
    github_issue: null
    date_reported: "2026-XX-XX"

  - anomaly_id: "MBCA-ANO-003"
    title: "USD results layer left in partially-written state on physics process crash"
    description: >
      If the PhysicsProcess backend terminates unexpectedly (crash, OOM,
      network loss in cloud mode) while ResultsApplicator::apply() is
      mid-write, the USD results layer may contain a mix of current-frame
      and previous-frame values. The partially-written results layer
      remains visible in the viewport until the user manually reloads the
      scene or clears the results layer.
    affected_components: ["ResultsApplicator", "SceneManager", "SimulationController"]
    affected_versions: "0.1.0 – current"
    severity: "major"
    safety_relevance: "context_dependent"
    safety_relevance_rationale: >
      Stale position data from a prior frame may be displayed as current,
      potentially misrepresenting a microrobot's last known state to the
      operator. In a clinical context, an operator relying on the displayed
      position to make a treatment decision could be misled by stale data.
      The downstream manufacturer must implement crash detection and
      automatic results layer clearing.
    workaround: "Reload the scene to clear the results layer after a backend crash"
    resolution_status: "open"
    resolution_verification: null            # crash recovery requires SimulationController changes
    detected_by: "manual_review"
    memory_safety_relevant: true             # mid-write crash may leave SdfLayer inconsistent
    github_issue: null
    date_reported: "2026-XX-XX"

  - anomaly_id: "MBCA-ANO-004"
    title: "PhysicsProcess::receiveResult() blocks indefinitely on backend hang"
    description: >
      PhysicsProcess::receiveResult() is a blocking call with no timeout.
      If the physics backend (MADDENING/MIME) enters an infinite loop,
      deadlocks, or becomes unresponsive without closing the connection,
      receiveResult() will block indefinitely. Because this call executes
      on the simulation thread which holds resources needed by the Qt
      event loop, the entire application may become unresponsive with no
      recovery path other than process termination (kill -9).
    affected_components: ["PhysicsProcess", "SimulationController"]
    affected_versions: "0.1.0 – current"
    severity: "major"
    safety_relevance: "context_dependent"
    safety_relevance_rationale: >
      In a clinical context, an unrecoverable application hang during a
      live procedure would prevent the operator from accessing simulation
      output or controlling the simulation. The downstream manufacturer
      must implement a timeout mechanism (e.g., ZeroMQ RCVTIMEO socket
      option) and a watchdog that detects backend unresponsiveness and
      triggers a controlled shutdown of the simulation session.
    workaround: "Terminate the application process if it becomes unresponsive"
    resolution_status: "open"
    resolution_verification: null            # timeout requires ZmqPhysicsProcess implementation
    detected_by: "manual_review"
    memory_safety_relevant: false
    github_issue: null
    date_reported: "2026-XX-XX"
```

**Schema fields added beyond MADDENING's base schema:**

- `resolution_verification` (optional, string): The `MBCA-VER-*` benchmark ID that must pass before this anomaly can be marked `"fixed"`. Null if no verification benchmark exists yet. The validator (`check_anomalies.py`) checks that when `resolution_status` is `"fixed"`, the `resolution_verification` field is non-null and references a valid `MBCA-VER-*` ID that exists in the verification benchmark registry. An anomaly marked `"fixed"` without a passing verification benchmark fails the release gate.

- `detected_by` (optional, string): Tool or process that found the anomaly. Values: `"asan"` | `"ubsan"` | `"tsan"` | `"valgrind"` | `"manual_review"` | `"ci_test"` | `"user_report"`. Allows the SOUP package summary to count anomalies by detection method.

- `memory_safety_relevant` (optional, boolean, default `false`): `true` if this anomaly involves undefined behaviour, memory corruption, or a data race. When `memory_safety_relevant: true`, the anomaly's `safety_relevance` must be explicitly set (not omitted) — memory safety anomalies may not default to `"not_safety_relevant"` without a documented rationale. This field enables separate counting of memory-safety-relevant anomalies in the SOUP package summary — a metric a Notified Body reviewer is likely to ask about specifically.

#### GitHub Issues Label Taxonomy

Identical to MADDENING:

| Label | Colour | Meaning |
|---|---|---|
| `anomaly:critical` | Red | Incorrect behaviour, no workaround |
| `anomaly:major` | Orange | Incorrect behaviour, workaround available |
| `anomaly:minor` | Yellow | Cosmetic or minor issue |
| `safety-relevant` | Purple | Could affect safety-critical downstream use |
| `soup-assessment` | Blue | Relevant to IEC 62304 SOUP evaluation |
| `known-anomaly` | Grey | Tracked in known_anomalies.yaml |

#### Three-Tier Release Gate

Identical to MADDENING Section 9.7:

- **Tier 1 — `safety-relevant`**: Hard gate, no grace period
- **Tier 2 — `anomaly:critical` or `anomaly:major`**: No grace period
- **Tier 3 — `anomaly:minor`**: Two-cycle grace period

CI validation via `scripts/check_anomalies.py` (adapted from MADDENING's `maddening.compliance.validate_anomaly_registry()` — MICROBOTICA calls it with `--prefix MBCA-ANO-`).

---

### 8.7 ISO 14971 Risk Management Hooks

#### Inheritance Statement

This section **adapts** MADDENING Section 9.8 for interface boundaries rather than physics nodes. The technical-vs-clinical boundary principle is preserved identically.

#### Resolution of Concern 2 (Hazard Hints on Interface Boundaries vs. Implementations)

Hazard hints live on **both** the abstract interface and concrete implementations, with clearly delineated scope:

**Abstract interface hazard hints** document failure modes inherent to the interface contract itself — conditions that any implementation must contend with. Example: "Undefined behaviour if `receiveResult()` is called before `launch()`" is a `PhysicsProcess` interface-level hazard because it applies to all implementations regardless of transport mechanism.

**Concrete implementation hazard hints** document failure modes specific to that implementation's technology choices. Example: "ZeroMQ transport drops frames if the remote process is killed mid-step" is a `ZmqPhysicsProcess` implementation-level hazard that does not apply to `StubPhysicsProcess`.

**How this interacts with the downstream manufacturer's ISO 14971 assessment**: When the manufacturer assesses MICROBOTICA as SOUP, they must evaluate hazard hints at both levels. The interface-level hints apply regardless of which implementation the manufacturer uses. The implementation-level hints apply only to the specific implementations they deploy. When the concrete implementation wraps an upstream SOUP component (e.g., `ZmqPhysicsProcess` communicates with MIME, which uses MADDENING), the implementation-level hazard hints should cross-reference upstream anomalies where relevant (e.g., "See MIME-ANO-003 for upstream anomaly affecting this transport path"). The manufacturer must assess the upstream anomaly through both MICROBOTICA's and MIME's SOUP packages.

#### Technical Hazard Hints vs. Clinical Risk Assessments

**Taken verbatim in principle from MADDENING Section 9.8**. The boundary is:

- MICROBOTICA provides **technical conditions** only — interface contract violations, transport failures, rendering pipeline limitations, scripting API boundary conditions
- MICROBOTICA does NOT provide **clinical risk assessments** — "could cause patient harm", "risk of neurological damage"
- The transition from technical hint to clinical hazard requires the manufacturer to supply clinical context, device-specific scenario, patient population, and ISO 14971 probability/severity estimation

---

### 8.8 Usability Engineering Documentation

#### Inheritance Statement

This section is **new** — it has no MADDENING equivalent. MICROBOTICA is all UI; MADDENING has no UI. This section addresses IEC 62366-1:2015 considerations.

#### Scope

IEC 62366-1:2015 applies to the commercial manufacturer's product, not to MICROBOTICA itself. MICROBOTICA is not a medical device and is not subject to usability engineering requirements. However, MICROBOTICA's UI design directly affects the **use error risks** that the downstream manufacturer must assess.

#### `docs/regulatory/usability_engineering.md`

This document must identify:

**1. UI Actions with Safety-Relevant Consequences**

| UI Action | Safety Consequence | Current Mitigation | Recommended Manufacturer Control |
|---|---|---|---|
| Switching physics backend (validated → unvalidated) | Simulation output no longer verified | Warning dialog | Disable in production mode or require authentication |
| Modifying parameters during live simulation | Physics backend receives unvalidated values | None (Phase 0) | Parameter whitelist + range validation in SimulationController |
| Executing arbitrary Python in scripting console | Can modify scene state and inject parameters | None | Sandboxed scripting mode in clinical deployment |
| Loading an untrusted USD scene file | Malicious scene could exploit USD parser | None | Scene validation and signing |
| Switching from local to cloud compute during simulation | Results may differ due to platform differences | None | Require simulation restart on backend switch |

**2. Out-of-Range Warning System**

When simulation output values exceed validated parameter regimes (as defined by MIME/MADDENING's `validated_regimes` metadata), MICROBOTICA should display a visual warning in the viewport. The specifics:

- Warning appears as a non-dismissible banner in the viewport widget
- Warning states which values are out of range and what the validated range is
- Warning is logged to the audit log
- Warning does NOT halt the simulation — that is an application-layer control decision for the manufacturer

**3. Scripting Console Boundaries During Live Simulation**

The `microrobotica` scripting module must define which operations are safe during active simulation:

| Operation | Safe During Simulation? | Rationale |
|---|---|---|
| `microrobotica.scene().prim_paths()` | Yes | Read-only |
| `microrobotica.scene().get_attribute(...)` | Yes | Read-only |
| `microrobotica.scene().set_attribute(...)` | **No** — writes to override layer | Could modify scene geometry while physics is running on stale geometry |
| `microrobotica.sim().current_time()` | Yes | Read-only |
| `microrobotica.sim().current_frame()` | Yes | Read-only |
| `microrobotica.sim().send_parameters(...)` | **Conditional** | Safe if parameters are within validated ranges; unsafe otherwise |

**Resolution of Concern 4 (Scripting console safety boundary)**: Parameter validation should be implemented at **two layers**:

1. **`SimulationController` (primary)**: A parameter whitelist and range check before forwarding to `PhysicsProcess`. This is the architectural enforcement point — it exists regardless of how parameters arrive (scripting console, UI widget, programmatic API). The whitelist is populated from MIME/MADDENING's `validated_regimes` metadata at simulation launch time.

2. **`MicrobotaModule.cpp` (secondary)**: Type validation (correct Python types for each parameter) and a runtime check that `send_parameters()` is not called when the simulation is in an unsafe state. This catches errors at the scripting API boundary before they reach `SimulationController`.

3. **Documented limitation for downstream manufacturer**: Even with both layers, the parameter validation is a **best-effort safeguard, not a safety barrier**. The validated ranges come from MIME/MADDENING and may not cover all possible dangerous parameter combinations. The downstream manufacturer must implement their own parameter governance (potentially hardware-in-the-loop validation or formal parameter approval workflows) for clinical deployment. This limitation is documented in the component guide for `SimulationController` and in the SOUP package.

---

## 9. Rendering Pipeline Correctness Documentation

### Inheritance Statement

This section is **new** — it has no MADDENING equivalent. The USD layer separation architecture is a safety-relevant design decision unique to MICROBOTICA.

### The Three-Layer USD Architecture

MICROBOTICA enforces a three-layer USD composition stack:

```
Layer 3 (strongest): Results Layer (session sublayer)
    Written by: ResultsApplicator ONLY
    Contains: simulation output (prim transforms, scalar fields)
    Persisted: NEVER — destroyed when session ends
    Mutability: overwritten every frame

Layer 2 (middle): Override Layer (user sublayer)
    Written by: microrobotica.scene().set_attribute() ONLY
    Contains: user parameter overrides, annotation prims
    Persisted: optionally, as a .usd sidecar file
    Mutability: modified by user scripting commands

Layer 1 (weakest): Base Layer (scene file)
    Written by: scene file loader ONLY
    Contains: original scene geometry, materials, initial state
    Persisted: always — this IS the scene file
    Mutability: NEVER modified after load
```

### Why This Is Safety-Relevant

USD composition semantics mean that a stronger layer's opinion overrides a weaker layer's. This architecture ensures:

1. **Simulation output never contaminates the base scene**: If the simulation writes a wildly incorrect position, the base geometry is unaffected. Restarting the simulation (clearing the results layer) restores the base state exactly.

2. **User overrides are separable from simulation results**: A user script that modifies a parameter can be undone by removing the override layer, without affecting either the base scene or current simulation results.

3. **The base scene is immutable after load**: This provides a clean "known-good state" that can always be restored. In a clinical context, this means the anatomical geometry (e.g., CSF ventricle mesh from imaging data) is never modified by simulation output or user scripting.

### Verification Evidence

The `test_layer_stack.cpp` verification test suite (seed exists in Phase 0) must verify:

| Test | What It Proves | Benchmark ID |
|---|---|---|
| Base layer is never written by ResultsApplicator | Simulation output cannot contaminate base scene | MBCA-VER-001 |
| Results layer is not persisted on scene save | Results are session-only | MBCA-VER-002 |
| ResultFrame position maps exactly to USD xformOp:translate | Data integrity through the rendering pipeline | MBCA-VER-003 |
| ResultsApplicator logs warning for unknown prim paths | No silent data loss (fixes MBCA-ANO-001) | MBCA-VER-004 |
| `set_attribute()` writes only to override layer | Scripting cannot contaminate base or results | MBCA-VER-005 |
| Orientation values map exactly to USD xformOp:orient | Orientation data integrity through the rendering pipeline | MBCA-VER-007 |
| MeshData vertexColors map exactly to primvars:displayColor | Vertex-color data integrity through the rendering pipeline | MBCA-VER-008 |
| ResultFrame JSON survives ZMQ PUB/SUB transport | IPC protocol fidelity through the ZMQ transport | MBCA-VER-009 |
| MimeStubPhysicsProcess data arrives at USD prims via demo scene | End-to-end data integrity through the full pipeline | MBCA-VER-010 |

### Component Guide Entry

`docs/component_guide/implementations/scene_manager.md` and `docs/component_guide/implementations/results_applicator.md` must document the three-layer architecture, the invariants it enforces, and the verification evidence that supports each invariant.

### Crash Recovery (Known Limitation — MBCA-ANO-003)

If the `PhysicsProcess` backend terminates unexpectedly (crash, OOM, network loss in cloud mode), the USD results layer may be left in a partially-written state. The results layer is an in-memory `SdfLayer` with the most recently applied `ResultFrame` values — these may be mid-write if the crash occurs during `ResultsApplicator::apply()`. Recovery requires: (1) stopping the simulation loop, (2) clearing the results layer via `resultsLayer->Clear()`, (3) reloading the base layer. MICROBOTICA does not implement automatic crash recovery in Phase 0. The `SimulationController` should catch the exception from `PhysicsProcess::receiveResult()` (which will throw or return `std::nullopt` when the backend dies) and emit a Qt signal that triggers the crash recovery UI flow. This recovery path must be implemented before MIME integration — it is a Phase 1 requirement, not Phase 2.

---

## 10. Python Scripting API Contract

### Inheritance Statement

This section is **new** — it has no MADDENING equivalent. The `microrobotica` Python module is the surface most likely to be misused and requires explicit contract documentation.

### `docs/component_guide/scripting_api.md`

#### API Surface

```python
import microrobotica

# Scene access (read-only during simulation)
microrobotica.scene().prim_paths()                      # → list[str]
microrobotica.scene().get_attribute(prim, attr)          # → value
microrobotica.scene().set_attribute(prim, attr, value)   # → None (override layer)

# Simulation access
microrobotica.sim().current_time()                       # → float
microrobotica.sim().current_frame()                      # → dict (mirrors ResultFrame)
microrobotica.sim().send_parameters(params_dict)         # → None
microrobotica.sim().status()                             # → str ("idle"|"running"|"paused"|"error")
```

#### Complete Operation Safety Table

This is the authoritative table for which operations are safe during active simulation. The summary table in Section 8.8 is a subset — this table is canonical.

| Operation | Safe During Simulation? | Phase 0 Enforcement | Required Manufacturer Control |
|---|---|---|---|
| `microrobotica.scene().prim_paths()` | Yes — read-only | None needed | None |
| `microrobotica.scene().get_attribute(path, attr)` | Yes — read-only | None needed | None |
| `microrobotica.scene().set_attribute(path, attr, val)` | **No** — writes override layer | Documented limitation only | Disable scripting console during active simulation, or implement scene locking |
| `microrobotica.sim().current_time()` | Yes — read-only | None needed | None |
| `microrobotica.sim().current_frame()` | Yes — read-only | None needed | None |
| `microrobotica.sim().send_parameters(params)` | **Conditional** — type-checked only | Type validation in MicrobotaModule.cpp; range validation deferred | Parameter whitelist + validated range enforcement |
| `microrobotica.sim().status()` | Yes — read-only | None needed | None |

#### Contract Rules

1. **Layer isolation**: `set_attribute()` writes to the override layer ONLY. Attempting to write to a prim path that is managed by the results layer raises `RuntimeError`.

2. **Simulation state safety**: `set_attribute()` raises `RuntimeError` when called during an active simulation (state == `Running`). The simulation must be paused or stopped before scene modifications.

3. **Parameter validation**: `send_parameters()` validates:
   - All keys are `str`
   - All values are JSON-serializable
   - If a parameter whitelist is active (populated from MIME/MADDENING `validated_regimes`), out-of-range values raise `ValueError` with a descriptive message including the validated range

4. **Thread safety**: All `microrobotica` module calls are dispatched to the Qt main thread via `QMetaObject::invokeMethod()` with `Qt::BlockingQueuedConnection`. The scripting console runs on a separate Python thread; all mutations are serialized through Qt's event loop.

5. **Error handling**: Invalid arguments during an active session produce Python exceptions (`RuntimeError`, `ValueError`, `TypeError`), never C++ crashes. The scripting engine catches all exceptions and displays them in `ConsoleWidget`. No exception causes the simulation to enter an undefined state.

#### GIL and Qt Main Thread Constraint

The `microrobotica` module executes on the Qt main thread, holding the Python GIL for the duration of each call. This means:

1. Scripts that run long computations block the Qt event loop (see `MBCA-ANO-002`)
2. Qt signals are not processed during script execution
3. The module must never be called from a worker thread — doing so without explicit GIL management will cause undefined behaviour

This is a **known architectural constraint** in Phase 0, acceptable for a research tool. A future async scripting mode (executing scripts in a sub-interpreter on a worker thread with proper GIL release) is the long-term solution, deferred until Phase 2 or later.

#### Future Standalone Library (`microrobotica` on PyPI)

The `microrobotica` embedded module is compiled from `src/scripting/MicrobotaModule.cpp` using `PYBIND11_EMBEDDED_MODULE`. The same source file will be compiled as a standalone pybind11 extension module when `-DBUILD_PYTHON_MODULE=ON` is passed to CMake. The CMake target is:

```cmake
if(BUILD_PYTHON_MODULE)
    pybind11_add_module(microrobotica src/scripting/MicrobotaModule.cpp)
    target_include_directories(microrobotica PRIVATE src)
endif()
```

This proves the future library is buildable from day one. The standalone library will be published to PyPI as `microrobotica` and will allow external Python scripts and Jupyter notebooks to drive MICROBOTICA sessions programmatically (connecting to a running MICROBOTICA instance via a local socket). The API contract defined in this section is the same API contract the standalone library will expose.

The single-source-file design (`MicrobotaModule.cpp` used for both embedded and standalone) means the API cannot diverge between the two deployment modes. Any change to the embedded API is automatically reflected in the standalone library at the next build.

---

## 11. Cloud Provenance Documentation

### Inheritance Statement

This section is **new** — it has no MADDENING equivalent. When MADDENING/MIME runs on SkyPilot cloud infrastructure and results stream back to MICROBOTICA, the provenance chain is longer and must be captured.

### Cloud Provenance Fields

When MICROBOTICA connects to a remote compute backend (via `SkyPilotComputeBackend`), the audit logger must capture:

| Field | Source | Why It Matters |
|---|---|---|
| Cloud provider (AWS/GCP/Azure) | SkyPilot job metadata | Reproducibility: different providers may have different GPU hardware |
| SkyPilot job ID | SkyPilot API | Traceability: maps MICROBOTICA session to cloud job |
| Container image hash | Docker inspect on remote | Ensures the exact MIME/MADDENING versions are known |
| MIME version | IPC handshake response | SOUP-of-SOUP identification |
| MADDENING version | IPC handshake response | SOUP-of-SOUP identification |
| GPU hardware model | Remote system info | Platform-dependent numerical behaviour |
| Network round-trip time (RTT) | ZeroMQ heartbeat | Detects latency that could affect real-time use |
| Frames dropped | Sequence number gaps | Data integrity — any dropped frame is a provenance gap |
| Frames reordered | Sequence number ordering | Data integrity — reordered frames indicate network issues |

### Provenance Gap Policy

If any frames are dropped or reordered during a cloud compute session, the session provenance record must flag this. The audit log records the gap; the session summary includes `frames_dropped` and `frames_reordered` counts. MICROBOTICA does not attempt to interpolate or reconstruct missing frames — it renders what it receives and logs what it doesn't.

In a clinical context, the downstream manufacturer must decide whether dropped frames are acceptable in their context of use (they likely are for offline pre-operative planning; they are not for real-time intraoperative use).

---

## 12. IEC 62304 SOUP Compliance Package

### Inheritance Statement

**Taken verbatim in structure** from MADDENING Section 10. The SOUP package template is identical, with MICROBOTICA-specific content.

### SOUP Package Document Template

`docs/validation/soup_package.md`:

```markdown
# MICROBOTICA SOUP Package — v[X.Y.Z]

## 1. Software Identification (IEC 62304 Clause 5.3.3)

| Field | Value |
|-------|-------|
| Name | MICROBOTICA |
| Full name | MICROROBOTics Iterative simulation for Clinical Adoption |
| Version | [X.Y.Z] |
| Release date | [YYYY-MM-DD] |
| Licence | AGPL-3.0-or-later |
| Source repository | https://github.com/[org]/MICROBOTICA |
| SHA-256 (source tarball) | [hash] |
| Build system | CMake 3.25+, GCC 13, Ubuntu 24.04 |
| Runtime dependencies | Qt 6.6, OpenUSD 24.08, pybind11 2.12 |

## 2. Functional Description (IEC 62304 Clause 5.3.4 support)

MICROBOTICA provides:

### Core Application
- USD-based 3D scene management with three-layer composition
- Dual viewport: local Hydra/Storm rendering and Selkies WebRTC streaming
- Simulation timeline control via SimulationController
- Embedded Python scripting console (`microrobotica` module)

### Integration Interfaces (src/core/)
- PhysicsProcess: abstract physics computation backend
- RenderBackend / RenderSession: abstract rendering backend
- ComputeBackend: abstract compute resource management
- ResultFrame: structured simulation output
- PhysicsConfig: validated simulation configuration

### Capabilities NOT provided
- MICROBOTICA does not perform physics simulation
- MICROBOTICA does not interpret simulation results clinically
- MICROBOTICA does not provide diagnostic or therapeutic output
- MICROBOTICA does not validate that simulation parameters match
  any real physical system
- MICROBOTICA does not enforce safety limits on user-provided
  parameters (see Section 8.8 for the scripting safety boundary)
- MICROBOTICA does not currently surface biocompatibility simulation
  output (protein adsorption, coating degradation) — this is planned
  for Phase 2 when MIME's biocompatibility nodes are integrated

## 3. Known Anomalies (IEC 62304 Clauses 7.1.2, 7.1.3)

See `docs/validation/known_anomalies.yaml` for the complete,
machine-readable known anomalies registry for this release.

Summary of [N] known anomalies:
- Critical: [N] (of which safety-relevant: [N])
- Major: [N] (of which safety-relevant: [N])
- Minor: [N]
- Context-dependent safety relevance: [N]

## 4. Verification Evidence (IEC 62304 Clause 5.5/5.6 support)

### Test Suite Summary
- Total tests: [N]
- Verification tests: [N]
- Integration tests: [N]
- Pass rate: [N]% (release CI)
- Platforms tested: [list]
- CI system: [description, link]

### Verification Traceability (Class C)

| Component | Verification Test | Acceptance Criterion | Result |
|---|---|---|---|
| SceneManager | MBCA-VER-001 | Base layer never written by ResultsApplicator | PASS |
| SceneManager | MBCA-VER-002 | Results layer never persisted to disk | PASS |
| ResultsApplicator | MBCA-VER-003 | Position values map exactly to xformOp:translate | PASS |
| ResultsApplicator | MBCA-VER-004 | Warning logged for unknown prim paths | FAIL (expected — see MBCA-ANO-001) |
| MicrobotaModule | MBCA-VER-005 | set_attribute() writes to override layer only | PASS |
| StubPhysicsProcess | MBCA-VER-006 | simTime monotonically increasing | PASS |
| ResultsApplicator | MBCA-VER-007 | Orientation values map exactly to xformOp:orient | PASS |
| ResultsApplicator | MBCA-VER-008 | MeshData vertexColors map exactly to primvars:displayColor | PASS |
| MimePhysicsProcess | MBCA-VER-009 | ResultFrame JSON survives ZMQ PUB/SUB transport | PASS |
| MimeStubPhysicsProcess | MBCA-VER-010 | Data arrives at USD prims via demo scene pipeline | PASS |

## 5. IEC 62304 Lifecycle Activities Performed

[See Section 13 for the full mapping.]

## 6. Configuration Management

- All source code in Git with complete history
- Tagged releases on GitHub
- SHA-256 hashes for source tarballs
- CITATION.cff for citation and version identification
- SBOM available at [path]

## 7. Anomaly Management Policy

[Same structure as MADDENING Section 10, §7]

## 8. Dependencies (SOUP of SOUP)

### 8.1 MIME

| Field | Value |
|-------|-------|
| Name | MIME |
| Version | [X.Y.Z] (pinned) |
| Licence | LGPL-3.0-or-later |
| SOUP package | [URL to MIME's soup_package.md, version-tagged] |
| Known anomalies | [URL to MIME's known_anomalies.yaml, version-tagged] |

MIME in turn depends on MADDENING. See MIME's SOUP package
Section 8 for the MADDENING dependency chain.

### 8.2 Direct Dependencies

| Dependency | Version | Licence | Purpose |
|---|---|---|---|
| Qt | 6.6.x | LGPL-3.0 | UI framework, OpenGL, WebEngine |
| OpenUSD | 24.08 | Modified Apache-2.0 | Scene format, Hydra/Storm renderer |
| pybind11 | 2.12.x | BSD-3-Clause | Embedded Python interpreter |
| ZeroMQ/cppzmq | 4.x | MPL-2.0 / MIT | IPC to physics backends |
| spdlog | 1.x | MIT | Logging |
| nlohmann_json | 3.x | MIT | JSON serialization |
| Catch2 | 3.x | BSL-1.0 | Testing framework |
```

### SOUP-of-SOUP Chain

The MICROBOTICA SOUP chain is the longest in the ecosystem:

```
Commercial Product
  └── MICROBOTICA (SOUP)  ← assessed directly
      ├── Qt 6.6 (SOUP of SOUP)
      ├── OpenUSD 24.08 (SOUP of SOUP)
      ├── pybind11 (SOUP of SOUP)
      ├── ZeroMQ (SOUP of SOUP)
      └── MIME (SOUP of SOUP)
          └── MADDENING (SOUP of SOUP of SOUP)
              ├── JAX (SOUP³)
              └── NumPy (SOUP³)
```

The manufacturer must assess the entire chain. MICROBOTICA's SOUP package Section 8 provides the first level of chain documentation; MIME's SOUP package Section 8 provides the second level (MADDENING); and MADDENING's SOUP package Section 8 provides the third level (JAX, NumPy).

---

## 13. IEC 62304 Software Lifecycle Documentation Mapping

### Inheritance Statement

**Adapts** MADDENING Section 11 for C++/Qt artifacts. The IEC 62304 clause structure is identical; the evidence artifacts change.

### Scope

**MICROBOTICA is not subject to IEC 62304.** This mapping is provided voluntarily to support downstream SOUP assessment.

### Mapping

| IEC 62304 Phase | Clause | What the Standard Requires | What MICROBOTICA Provides | Gaps |
|---|---|---|---|---|
| **Software development planning** | 5.1 | Development plan, standards, tools | `DESIGN.md`, `ROADMAP.md`, `CONTRIBUTING.md`, CMakeLists.txt, CI YAML | No formal development plan in IEC 62304 format |
| **Software requirements analysis** | 5.2 | Documented requirements | `DESIGN.md`, interface contracts in `src/core/`, test suite | Requirements implicit in tests, not formal spec with IDs |
| **Software architectural design** | 5.3 | Architecture document, SOUP identification | `DESIGN.md`, `src/core/` interfaces, `ComponentMeta` on all interfaces | Well-documented architecture |
| **↳ Segregation analysis** (Class C) | 5.3.5 | Failure isolation evidence | Three-layer USD architecture (Section 9), `src/core/` interfaces are pure virtual with no shared mutable state, Qt signal/slot for loose coupling | USD layer separation is an architecturally enforced segregation boundary |
| **Software detailed design** | 5.4 | Detailed design per unit | Component guide documents, Doxygen comments, `ComponentMeta` | Coverage grows as component guide is populated |
| **Software unit implementation** | 5.5 | Implementation per design | Source code in `src/`, Doxygen-documented | Code is structured by architectural layer |
| **Software unit verification** | 5.6 | Unit verification evidence | `tests/` with Catch2, `tests/verification/` for formal benchmarks; ASan/UBSan on every PR; TSan nightly; Valgrind weekly on verification suite | Memory safety analysis is CI-enforced; errors enter the anomaly registry |
| **Software integration testing** | 5.7 | Integration test evidence | Integration tests exercising interface → implementation paths | Present |
| **Software system testing** | 5.8 | System-level testing | End-to-end session tests (load scene → run sim → verify output) | Examples serve as system tests |
| **Software release** | 5.9 | Release documentation | `CHANGELOG.md`, tagged releases, `known_anomalies.yaml`, SOUP package | Once SOUP package is populated, well-covered |

### Unit Decomposition

For IEC 62304 Clause 5.3, the unit decomposition of MICROBOTICA is:

| Software Unit | Header(s) | Responsibility | Component ID |
|---|---|---|---|
| `PhysicsProcess` | `src/core/physics_process.h` | Abstract physics backend interface | MBCA-COMP-001 |
| `RenderBackend` | `src/core/render_backend.h` | Abstract interface for local rendering backends | MBCA-COMP-002 |
| `RenderSession` | `src/core/render_session.h` | Abstract interface for cloud streaming render sessions | MBCA-COMP-003 |
| `ComputeBackend` | `src/core/compute_backend.h` | Abstract interface for compute resource management | MBCA-COMP-004 |
| `ResultFrame` | `src/core/result_frame.h` | Structured simulation output data type | MBCA-COMP-005 |
| `PhysicsConfig` | `src/core/physics_config.h` | Validated simulation configuration data type | MBCA-COMP-006 |
| `SceneManager` | `src/scene/scene_manager.h` | USD stage ownership and three-layer composition enforcement | MBCA-COMP-010 |
| `ResultsApplicator` | `src/scene/results_applicator.h` | ResultFrame to USD prim attribute mapping | MBCA-COMP-011 |
| `SimulationController` | `src/simulation/simulation_controller.h` | Timeline control and PhysicsProcess orchestration | MBCA-COMP-020 |
| `ViewportWidget` | `src/viewport/viewport_widget.h` | Local/stream viewport switching | MBCA-COMP-030 |
| `MicrobotaModule` | `src/scripting/MicrobotaModule.cpp` | `microrobotica` Python scripting API (embedded + future standalone) | MBCA-COMP-040 |
| `ConsoleWidget` | `src/scripting/console_widget.h` | In-app Python REPL panel | MBCA-COMP-041 |

The `src/core/` interfaces are the primary unit decomposition artifact. Each interface defines an architectural boundary; each concrete implementation is a sub-unit.

---

## 14. EU MDR Annex I and Annex II Alignment

### Inheritance Statement

**Taken verbatim in structure** from MADDENING Section 12. The same Annex I GSPRs and Annex II sections apply; the evidence artifacts change.

### What MICROBOTICA Provides to the Technical File

| Annex II Section | What MICROBOTICA Provides |
|---|---|
| §2 — Information supplied by manufacturer | `intended_use.md`, `downstream_integration.md`, README disclaimer |
| §4 — Design and manufacturing information | `DESIGN.md`, `docs/component_guide/`, `docs/validation/`, `soup_package.md`, IEC 62304 mapping |
| §6 — Benefit-risk analysis | Component hazard hints, `known_anomalies.yaml`, validated regimes |

### GSPR 17 — Software Requirements

**17.1**: MICROBOTICA's state-of-the-art evidence: published verification tests, systematic architecture (abstract interfaces in `src/core/`), established rendering technology (OpenUSD, Hydra/Storm), peer-reviewed scripting interface pattern (pybind11).

**17.3**: SOUP package Section 1 includes hardware/OS requirements. `SECURITY.md` documents vulnerability reporting. SBOM artifacts support cybersecurity assessment per MDCG 2019-16.

---

## 15. MDCG 2019-11: Qualification and Classification

### Inheritance Statement

**Taken verbatim in structure** from MADDENING Section 13, with MICROBOTICA-specific analysis.

### Step 1: Is MICROBOTICA a Medical Device?

**MICROBOTICA is NOT a medical device** because:

1. **No medical purpose**: MICROBOTICA is a research simulator. It displays physics simulation output in a 3D viewport and provides scripting access to scene and simulation state. It does not perform any function listed in EU MDR Article 2(1).

2. **Research platform**: MDCG 2019-11 Section 3.2 explicitly states that "generic tools or general purpose software [...] are not in themselves medical devices." MICROBOTICA is analogous to 3D Slicer, ParaView, or NVIDIA Omniverse for research.

3. **No clinical interpretation**: MICROBOTICA renders numerical simulation output. It does not interpret results in a clinical context, provide diagnostic conclusions, or make therapeutic recommendations.

### Step 2: Commercial Product Classification

A commercial product built on MICROBOTICA with a medical purpose would be classified identically to the analysis in MADDENING Section 13: Class IIb minimum for intraoperative digital twin use; Class III for RL policy generation for active microrobot control.

MICROBOTICA's position at Layer 3 makes it the most proximate open-source component to the commercial product. The manufacturer's classification dossier will reference MICROBOTICA's SOUP package as the primary SOUP assessment, with MIME and MADDENING as SOUP-of-SOUP dependencies assessed through MICROBOTICA's SOUP package chain documentation.

---

## 16. Configuration Management

### Inheritance Statement

**Adapts** MADDENING Section 14 for C++ distribution. No PyPI; uses source tarballs and CMake.

### What MICROBOTICA Provides

- **GitHub release tags**: Each version tagged in Git with SHA-256 hashes for source tarballs
- **CITATION.cff**: Academic citation and configuration management artifact
- **SessionProvenance**: Captures `microbotica_version` at runtime (Section 8.2)
- **SBOM**: CycloneDX format, generated from `CMakeLists.txt` dependency declarations and `vcpkg.json` / `conanfile.txt` (whichever package manager is used)

### `CITATION.cff`

```yaml
cff-version: 1.2.0
message: "If you use MICROBOTICA, please cite it as below."
title: "MICROBOTICA"
type: software
version: "0.1.0"
date-released: "2026-XX-XX"
license: "AGPL-3.0-or-later"
repository-code: "https://github.com/[org]/MICROBOTICA"
authors:
  - family-names: "[Author]"
    given-names: "[Author]"
    orcid: "https://orcid.org/XXXX-XXXX-XXXX-XXXX"
```

---

## 17. QMS Compatibility

### Inheritance Statement

**Taken verbatim** from MADDENING Section 15 — same model, same rationale. MICROBOTICA is a solo research project that does not operate under an ISO 13485 QMS. The same substitution table applies (Git for version control, CI for verification, GitHub Issues for corrective action, etc.).

---

## 18. Downstream Integration Guide

### Inheritance Statement

This section is **new from MICROBOTICA's perspective** — it is the counterpart to MADDENING's `downstream_integration.md`, but written from Layer 3's viewpoint (the layer most likely to be the direct dependency of a commercial product).

### `docs/regulatory/downstream_integration.md`

This document describes how a commercial manufacturer building on MICROBOTICA inherits from all three upstream layers:

```
Commercial Product (CE-marked SaMD)
    ├── MICROBOTICA (Layer 3, AGPL-3.0, SOUP)
    │   ├── Qt, OpenUSD, pybind11, ZeroMQ (SOUP of SOUP)
    │   └── MIME (Layer 2, LGPL-3.0, SOUP of SOUP)
    │       └── MADDENING (Layer 1, LGPL-3.0, SOUP of SOUP of SOUP)
    │           └── JAX, NumPy (SOUP³)
    └── Manufacturer's own code (clinical interpretation, UI customization,
        safety controls, regulatory compliance layer)
```

#### What MICROBOTICA's SOUP Package Provides to the Manufacturer

| Manufacturer's Need | MICROBOTICA Provides |
|---|---|
| Software identification (IEC 62304 §5.3.3) | SOUP package §1 with version, hash, dependencies |
| Functional specification (IEC 62304 §5.3.4) | SOUP package §2 with capability list and "NOT provided" list |
| Known anomalies (IEC 62304 §7.1.2) | `known_anomalies.yaml` with MBCA-ANO-* entries |
| Architecture for SOUP assessment (IEC 62304 §5.3) | `DESIGN.md`, component guide, `src/core/` interfaces |
| Segregation evidence (IEC 62304 §5.3.5) | Three-layer USD architecture, `src/core/` pure virtual interfaces |
| V&V evidence (IEC 62304 §5.5/5.6) | Catch2 verification tests, verification traceability table |
| SOUP chain (MIME + MADDENING) | SOUP package §8 with pinned versions and cross-references |
| Usability engineering input (IEC 62366) | `usability_engineering.md` with safety-relevant UI actions |
| Configuration management (IEC 62304 §8) | CITATION.cff, Git tags, SHA-256 hashes, SBOM |

#### What the Manufacturer Must Add Independently

| Requirement | Why MICROBOTICA Cannot Provide It |
|---|---|
| ISO 14971 risk management file | Risk assessment requires clinical context MICROBOTICA does not have |
| Clinical evaluation report (CER) | Clinical evidence requires clinical studies |
| IEC 62366-1 usability engineering file | Usability evaluation requires the manufacturer's own clinical UI |
| ISO 13485 QMS | QMS requires organisational structure |
| Notified Body relationship | Legal/financial obligation |
| Post-market surveillance (PMS, PSUR, PMCF) | Legal obligation requiring ongoing clinical monitoring |
| Parameter governance for clinical deployment | Clinical parameter ranges specific to the manufacturer's COU |
| Scripting console access controls | Authentication/authorisation for clinical contexts |
| Network security for cloud compute | TLS/CurveZMQ configuration for clinical deployment |
| Biocompatibility assessment for specific materials | Requires ISO 10993 testing; MIME provides simulation predictions only, not regulatory evidence |

#### AGPL-3.0 Considerations for Downstream Commercial Use

The AGPL-3.0 licence has specific implications for commercial deployment:

- **On-premises deployment** (MICROBOTICA runs on the manufacturer's hardware, no network interaction with external users): AGPL source disclosure obligation may not be triggered, subject to legal interpretation of "network interaction."
- **Cloud/SaaS deployment** (MICROBOTICA's functionality served over a network): AGPL requires complete source disclosure to network users.
- **Dual licensing**: The developer retains the option to offer commercial licences that waive the AGPL network disclosure requirement.

Commercial manufacturers should obtain legal counsel on AGPL compliance for their specific deployment model before incorporating MICROBOTICA.

---

## Appendix A: Inheritance from MADDENING Summary Table

| Section | Relationship to MADDENING | What Changed |
|---|---|---|
| §1 Documentation Structure | Adapted | Doxygen/Breathe instead of autodoc; component_guide instead of algorithm_guide; usability_engineering.md added |
| §2 Regulatory Boundary Language | Near-verbatim | AGPL instead of LGPL; Layer 3 framing; scripting console cybersecurity |
| §3 Component Documentation Standards | Adapted | Interface/implementation templates instead of algorithm templates; implementation map traces interface→impl not equation→code |
| §4 V&V Documentation Hooks | Adapted | Application-layer V&V scope; "faithful rendering" boundary; dependency acknowledgement language |
| §5 Versioning and API Stability | Verbatim | `ui:` commit prefix added |
| §6 Contributor Standards | Adapted | C++/CMake/Catch2 checklist instead of Python/pytest |
| §6 Memory Safety (addition) | **New** | No MADDENING equivalent (Python GC; different safety profile); ASan/UBSan/TSan/Valgrind suite for C++17; anomaly schema extended with `detected_by` and `memory_safety_relevant` fields |
| §7 README and Root Files | Near-verbatim | AGPL licence; C++ build instructions |
| §8.1 ComponentMeta | Adapted | C++ struct instead of Python dataclass; interface + implementation granularity |
| §8.2 Session Provenance | Adapted | Cloud provenance fields added; desktop session model |
| §8.3 Verification Test Registration | Adapted | Catch2 tags + C++ macro instead of Python decorator |
| §8.4 Deprecation/Stability | Adapted | C++ [[deprecated]] + spdlog instead of Python warnings |
| §8.5 Audit Logging | Adapted | spdlog backend; cloud provenance events; scripting events |
| §8.6 Anomaly Management | Verbatim | Prefix MBCA-ANO-* instead of MADD-ANO-* |
| §8.7 ISO 14971 Hazard Hints | Adapted | Interface + implementation dual-level scoping |
| §8.8 Usability Engineering | **New** | IEC 62366; UI safety actions; scripting boundaries |
| §9 Rendering Pipeline | **New** | Three-layer USD architecture; layer separation verification |
| §10 Scripting API Contract | **New** | `microrobotica` module safety boundaries; GIL/main-thread constraint; future standalone library boundary; complete operation safety table |
| §11 Cloud Provenance | **New** | SkyPilot/Selkies provenance; frame drop tracking |
| §12 SOUP Package | Near-verbatim | C++ dependencies; SOUP-of-SOUP-of-SOUP chain |
| §13 IEC 62304 Mapping | Adapted | CMake/Catch2 artifacts; three-layer USD segregation |
| §14 EU MDR Alignment | Near-verbatim | Same Annex I/II mapping |
| §15 MDCG 2019-11 | Near-verbatim | Layer 3 specifics |
| §16 Configuration Management | Adapted | Source tarballs instead of PyPI |
| §17 QMS Compatibility | Verbatim | Same model |
| §18 Downstream Integration | **New from MBCA perspective** | Three-layer SOUP chain; AGPL considerations |

---

## Appendix B: Implementation Priority

### Philosophy

Phase 0 establishes the proving shell and documentation foundation. Phases 1–2 build verification infrastructure and regulatory documents. Phase 3 adds cloud integration and community features. Phase 4 adds full documentation site and commercial-readiness artifacts.

### Phase 0: Foundation (implement now — the proving shell)

**Resolution of Concern 6 (Phase-gated documentation)**: Phase 0 documentation must exist before any code is written beyond the proving shell. This is the minimum set needed to establish the documentation discipline from day one.

0a. **`ComponentMeta` struct in `src/core/component_meta.h`** (Section 8.1) — define the struct and attach to all Phase 0 `src/core/` interfaces
0b. **`docs/validation/known_anomalies.yaml`** (Section 8.6) — start the anomaly registry immediately, even if empty
0c. **`CHANGELOG.md`** (Section 5) — start structured changelog
0d. **`CITATION.cff`** (Section 16) — citation and CM artifact
0e. **README disclaimer** (Section 7) — EU MDR-aware intended use statement
0f. **`docs/regulatory/intended_use.md`** (Section 2) — platform positioning statement
0g. **Skeleton `docs/regulatory/downstream_integration.md`** (Section 18) — four-layer chain + commercial boundary statement
0h. **GitHub Issues label taxonomy** (Section 8.6) — anomaly labels
0i. **`.github/ISSUE_TEMPLATE/anomaly.md`** (Section 8.6) — structured anomaly issue template
0j. **`scripts/check_anomalies.py`** (Section 8.6) — YAML validation (calls with `--prefix MBCA-ANO-`)
0k. **Skeleton `docs/validation/soup_package.md`** (Section 12) — Sections 1-3 at minimum

### Phase 1: Pre-MIME Integration (verification infrastructure)

> **MIME Integration Gate**: All Phase 1 items in this checklist must be complete before the first real MIME/MADDENING physics process is wired into MICROBOTICA. This gate is non-negotiable. When real physics data flows through MICROBOTICA for the first time, the verification infrastructure must already be in place. Integration errors that go undetected at first wiring become architectural debt. The integration gate is enforced by a CI check: a GitHub Actions workflow step that verifies the existence of all required Phase 1 files before any branch named `feat/mime-*` or `feat/maddening-*` can merge to main.

**Resolution of Concern 6 (minimum before MIME integration)**: Before the first real MIME integration, ALL Phase 0 items plus the following must exist.

1. **`tests/verification/` directory** with Catch2 verification tests — seed with `test_layer_stack.cpp`
2. **Component guide template** (`docs/component_guide/interfaces/_template.md`)
3. **Component guide for `PhysicsProcess`** — the first real interface documentation
4. **Component guide for `ResultsApplicator`** — the data integrity boundary
5. **Component guide for `SceneManager`** — the USD layer separation documentation
6. **Verification benchmark registration** (Section 8.3) — `REGISTER_VERIFICATION_BENCHMARK` macro
7. **`docs/validation/framework_verification.md`** — aggregate verification evidence
8. **Centralized `bibliography.bib`** and `check_citations.py` CI script
9. **`docs/regulatory/iec62304_mapping.md`** — IEC 62304 lifecycle mapping
10. **`docs/regulatory/usability_engineering.md`** (Section 8.8) — UI safety documentation
11. **`docs/component_guide/scripting_api.md`** (Section 10) — `microrobotica` module contract
12. **`SECURITY.md`** — vulnerability reporting
13. **Crash recovery path** — `SimulationController` handles `PhysicsProcess` crash by clearing the results layer and notifying the UI (see MBCA-ANO-003)

### Phase 2: Cloud Integration Documentation (when SkyPilot/Selkies integration begins)

14. **Session provenance** (Section 8.2) — `SessionProvenance` struct and audit logger integration
15. **Cloud provenance documentation** (Section 11) — SkyPilot/Selkies provenance fields
16. **`docs/component_guide/cloud_compute.md`** — SkyPilot/ZeroMQ/Selkies architecture
17. **IPC protocol verification tests** — `test_ipc_protocol.cpp`
18. **Audit logger implementation** (Section 8.5) — spdlog-based audit logger

### Phase 3: Full Regulatory Documentation (when commercial interest materialises)

19. **Complete `docs/regulatory/downstream_integration.md`** (Section 18) — full three-layer SOUP chain documentation
20. **`docs/regulatory/eu_mdr_guidelines.md`** (Section 14) — EU MDR alignment
21. **`docs/regulatory/mdcg_2019_11.md`** (Section 15) — qualification and classification
22. **`docs/validation/cou_template.md`** — Context of Use template
23. **SBOM generation** — CycloneDX from CMake/vcpkg
24. **Complete SOUP package** — all 8 sections fully populated
25. **Ontology integration forward reference** — when the MICROBOTICA registry adds SPARQL-based search (via MCO/Fuseki) and `BiocompatibilityMeta` display in the asset browser, a documentation architecture update will be required covering: the `docs/component_guide/` entries for registry browser components, the SOUP chain extension (Fuseki as a new SOUP dependency), and the `known_anomalies.yaml` scope for ontology query failures. This is deferred until the ontology layer design is finalised in the MIME and MICROBOTICA registry documentation architectures.

### Phase 4: Documentation Site and Commercial Readiness (when user base grows)

26. **Sphinx documentation build** — full docs site with Doxygen/Breathe API reference
27. **Per-component verification reports** — auto-generated from CI
28. **`GOVERNANCE.md`** — decision-making process
29. **`docs/regulatory/fda_guidelines.md`** — FDA alignment (secondary market)
30. **Stability machinery** (Section 8.4) — `[[deprecated]]` + registry
31. **Complete component guide coverage** — every `src/core/` interface and `src/` implementation documented

---

## Appendix C: Architectural Concern Resolutions

This appendix collects the resolutions to the six concerns raised in the project brief, with cross-references to where each resolution is implemented in the document.

### Concern 1: `ComponentMeta` Scope in C++

**Resolution**: `ComponentMeta` is a plain C++ struct in `src/core/component_meta.h`, using only standard library types and nlohmann_json. No Qt, USD, or Python dependencies. Granularity: one instance per abstract interface (contract-level) AND per concrete implementation (implementation-specific). See Section 8.1.

**Rationale**: A header-only struct is the simplest solution that achieves dependency-freedom. JSON sidecar files were considered but rejected because they would separate metadata from the code it describes, creating a documentation-rot risk that the C++ compiler cannot catch. A `constexpr` approach was considered but rejected because `std::string` is not `constexpr` in C++17.

### Concern 2: Hazard Hints on Interface Boundaries vs. Implementations

**Resolution**: Both. Interface-level hazard hints document contract-inherent failure modes (apply to all implementations). Implementation-level hazard hints document technology-specific failure modes (apply to that implementation only). Cross-references to upstream SOUP anomalies via anomaly ID strings (e.g., "See MIME-ANO-003"). See Section 8.7.

**Rationale**: The downstream manufacturer's ISO 14971 assessment must evaluate both levels. Interface-level hints are evaluated once regardless of implementation choice; implementation-level hints are evaluated per deployment configuration. This matches how a manufacturer would naturally perform hazard identification: first assess the architectural design (interfaces), then assess the detailed design (implementations).

### Concern 3: Verification Boundary Between MICROBOTICA and MIME/MADDENING

**Resolution**: MICROBOTICA's V&V explicitly acknowledges the dependency with the "faithful rendering" boundary language. MICROBOTICA verifies that data received is correctly rendered; MIME/MADDENING verify that data produced is physically correct. The conjunction of both is needed for end-to-end correctness. See Section 4.

**Rationale**: Over-claiming ("MICROBOTICA verifies physics") would misrepresent our V&V scope and could be challenged by a Notified Body. Under-claiming ("the rendering layer has no correctness obligation") would ignore that incorrect rendering of correct physics data is itself a hazard. The "faithful rendering" boundary — analogous to a medical display's fidelity obligation — is both technically accurate and regulatorily defensible.

### Concern 4: Scripting Console Safety Boundary

**Resolution**: Two-layer validation. Primary: `SimulationController` parameter whitelist + range check (architectural enforcement point). Secondary: `MicrobotaModule.cpp` type validation + state check (API boundary). Documented limitation: not a safety barrier; manufacturer must implement clinical parameter governance. See Section 8.8.

**Rationale**: Single-layer validation at the scripting API is insufficient because parameters can arrive via other paths (UI widgets, programmatic API). Single-layer validation at `SimulationController` is insufficient because type errors should be caught before they propagate. The "not a safety barrier" documentation is essential because validated ranges come from upstream SOUP and may not cover all dangerous parameter combinations.

### Concern 5: Inherited Anomaly Chain

**Resolution**: MICROBOTICA anomaly entries (`MBCA-ANO-*`) document only defects in MICROBOTICA's own C++ code and the **downstream impact** of upstream anomalies. An upstream MADDENING anomaly (e.g., LBM numerical instability) does NOT get a separate `MBCA-ANO-*` entry unless it manifests as a MICROBOTICA-specific failure mode (e.g., "viewport displays divergent results with no warning because the ResultFrame contains NaN values from upstream LBM instability"). See Section 8.6.

**Rationale**: Duplicating upstream anomalies would create a maintenance burden and risk inconsistency between registries. The correct scoping is: `MBCA-ANO-*` entries document what MICROBOTICA itself does wrong (or fails to detect) when upstream anomalies surface. The upstream anomaly itself is documented in MADDENING/MIME's registry and cross-referenced by ID string.

Cross-references to upstream anomalies appear in two places:
1. MICROBOTICA's SOUP package Section 8 — "MIME/MADDENING Anomalies Relevant to MICROBOTICA" table
2. Individual `MBCA-ANO-*` entries that describe MICROBOTICA-specific manifestations — the `safety_relevance_rationale` field references the upstream anomaly ID

### Concern 6: Phase-Gated Documentation

**Resolution**: Four-phase model aligned with MICROBOTICA's development phases. Phase 0 (proving shell): `ComponentMeta`, anomaly registry, intended use, SOUP package skeleton. Phase 1 (before MIME integration): verification tests, component guides for core interfaces, usability engineering, scripting contract. Phase 2 (cloud integration): session provenance, cloud provenance, IPC tests. Phase 3 (commercial interest): full regulatory documentation, complete SOUP package. Phase 4 (user base): documentation site, complete coverage. See Appendix B.

**Critical gate**: All Phase 0 + Phase 1 items must exist before the first real MIME integration. This ensures that when real physics data flows through MICROBOTICA for the first time, the verification infrastructure (layer separation tests, ResultsApplicator tests, IPC fidelity tests) is already in place. Without this gate, integration errors could go undetected and become entrenched in the codebase.

---

## Appendix D: Project Setup Checklist

### Phase 0 — Foundation

**Repository root files:**

- [ ] `CHANGELOG.md` exists with `Verification`, `Security`, and `Known Anomalies` sections (Section 5)
- [ ] `CITATION.cff` exists with `cff-version`, `title`, `type: software`, `version`, `date-released`, `license`, `repository-code`, `authors` (Section 16)
- [ ] `README.md` contains EU MDR-aware disclaimer (Section 7)

**docs/ directory structure:**

- [ ] `docs/regulatory/` directory exists
- [ ] `docs/validation/` directory exists
- [ ] `docs/component_guide/` directory exists

**docs/regulatory/intended_use.md:**

- [ ] File exists (Section 2)
- [ ] Contains Platform Positioning Statement identifying MICROBOTICA as research software, not a medical device under EU MDR Article 2(1) (Section 2)
- [ ] Contains Layered Responsibility Model table (Section 2)
- [ ] Contains Commercial Boundary Statement (Section 2)
- [ ] Contains Cybersecurity Boundary Statement (MDCG 2019-16) (Section 2)
- [ ] Contains AGPL Licence Statement (Section 2)

**docs/regulatory/downstream_integration.md (skeleton):**

- [ ] File exists (Section 18)
- [ ] Contains four-layer dependency chain (Section 18)
- [ ] Contains commercial responsibility statement (Section 18)
- [ ] Contains forward reference to full content in Phase 3 (Section 18)

**docs/validation/known_anomalies.yaml:**

- [ ] File exists and is valid YAML (Section 8.6)
- [ ] Contains `schema_version`, `microbotica_version`, `generated_date`, `anomalies` key (Section 8.6)

**docs/validation/soup_package.md (skeleton):**

- [ ] File exists with Sections 1-3 at minimum (Section 12)

**ComponentMeta:**

- [ ] `src/core/component_meta.h` exists with `ComponentMeta` struct (Section 8.1)
- [ ] `StabilityLevel` enum defined (Section 8.1)
- [ ] All Phase 0 `src/core/` interfaces have `ComponentMeta` attached via `interfaceMeta()` / `meta()` (Section 8.1)

**GitHub repository settings:**

- [ ] Labels: `anomaly:critical`, `anomaly:major`, `anomaly:minor`, `safety-relevant`, `soup-assessment`, `known-anomaly` (Section 8.6)
- [ ] `.github/ISSUE_TEMPLATE/anomaly.md` exists with mandatory fields (Section 8.6)

**CI:**

- [ ] `scripts/check_anomalies.py` exists and validates `known_anomalies.yaml` with `--prefix MBCA-ANO-` (Section 8.6)
- [ ] CI runs `scripts/check_anomalies.py` on every push (Section 8.6)

**Memory safety (Phase 0):**

- [ ] `CMakePresets.json` contains `linux-asan` preset with `-fsanitize=address,undefined` (Section 6)
- [ ] `lsan.suppressions` file exists suppressing known pybind11/CPython shutdown leaks (Section 6)
- [ ] CI runs `linux-asan` build on every pull request (Section 6)

### Phase 1 — Pre-MIME Integration (verification infrastructure)

> **MIME Integration Gate**: All Phase 1 items must be complete before the first real MIME/MADDENING physics process is wired into MICROBOTICA. This gate is enforced by a CI check that verifies required Phase 1 files exist before any `feat/mime-*` or `feat/maddening-*` branch can merge to main.

- [ ] `tests/verification/` directory exists with at least one Catch2 test (Section 8.3)
- [ ] `docs/component_guide/interfaces/_template.md` exists (Section 3)
- [ ] Component guides exist for: `PhysicsProcess`, `ResultsApplicator`, `SceneManager` (Sections 3, 9)
- [ ] `docs/component_guide/scripting_api.md` exists (Section 10)
- [ ] `docs/regulatory/usability_engineering.md` exists (Section 8.8)
- [ ] `docs/regulatory/iec62304_mapping.md` exists (Section 13)
- [ ] `docs/bibliography.bib` exists (Section 3)
- [ ] `scripts/check_citations.py` exists and runs in CI (Section 3)
- [ ] `docs/validation/framework_verification.md` exists (Section 4)
- [ ] `SECURITY.md` exists (Section 7)
- [ ] Verification benchmark registration macro exists in `src/core/verification_registry.h` (Section 8.3)
- [ ] At least one test registered via `REGISTER_VERIFICATION_BENCHMARK` (Section 8.3)
- [ ] `SimulationController` handles `PhysicsProcess` crash by clearing the results layer and notifying the UI (MBCA-ANO-003, Section 9)

**Memory safety (Phase 1):**

- [ ] `CMakePresets.json` contains `linux-tsan` preset with `-fsanitize=thread` (Section 6)
- [ ] `tsan.suppressions` file exists suppressing known Qt/Python dispatch patterns (Section 6)
- [ ] `valgrind.suppressions` file exists (Section 6)
- [ ] CI nightly job runs `linux-tsan` build (Section 6)

### Phase 2 — Cloud Integration

- [ ] `src/core/session_provenance.h` exists with `SessionProvenance` struct (Section 8.2)
- [ ] `src/core/audit_logger.h` exists with `AuditLogger` class (Section 8.5)
- [ ] `docs/component_guide/cloud_compute.md` exists (Section 11)
- [ ] `tests/verification/test_ipc_protocol.cpp` exists (Section 4)

### Phase 3 — Full Regulatory

- [ ] `docs/regulatory/downstream_integration.md` is complete (not skeleton) (Section 18)
- [ ] `docs/regulatory/eu_mdr_guidelines.md` exists (Section 14)
- [ ] `docs/regulatory/mdcg_2019_11.md` exists (Section 15)
- [ ] `docs/validation/cou_template.md` exists (Section 4)
- [ ] `docs/validation/soup_package.md` fully populated (Section 12)
- [ ] SBOM generation in release process (Section 16)

### Phase 4 — Documentation Site

- [ ] `docs/conf.py` exists (Sphinx) (Section 1)
- [ ] `Doxyfile` exists and generates API reference (Section 1)
- [ ] Every `src/core/` interface has a component guide document (Section 3)
- [ ] Every `src/` implementation has a component guide document (Section 3)
- [ ] `GOVERNANCE.md` exists (Section 7)
