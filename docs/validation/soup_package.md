# SOUP Package — MICROBOTICA

> **IEC 62304 Clause 8**: Software of Unknown Provenance assessment.

## 1. SOUP Identification

| Field | Value |
|-------|-------|
| Name | MICROBOTICA |
| Version | 0.1.0 |
| Licence | AGPL-3.0-or-later |
| Language | C++17 |
| Framework | Qt 6, OpenUSD 24.08 |
| Repository | https://github.com/MSF/MICROROBOTICA |
| Upstream SOUP | MADDENING (Layer 1), MIME (Layer 2) |
| Components | 11 (4 interfaces, 2 stub implementations, 5 app components) |
| Test cases | 32 (15 unit, 6 verification, 1 integration, 7 scripting, 3 stub render) |
| Known anomalies | 4 (0 critical, 3 major, 1 minor) |

## 2. Known Anomalies Summary

See `docs/validation/known_anomalies.yaml` for the complete registry.

| ID | Title | Severity | Safety Relevance | Status |
|----|-------|----------|-----------------|--------|
| MBCA-ANO-001 | ResultsApplicator silently drops unknown prim paths | Major | Context-dependent | Open |
| MBCA-ANO-002 | ScriptingEngine blocks Qt main thread | Minor | Context-dependent | Open |
| MBCA-ANO-003 | Results layer partially-written on crash | Major | Context-dependent | Open |
| MBCA-ANO-004 | receiveResult() blocks indefinitely on hang | Major | Context-dependent | Open |

## 3. Verification Evidence

| Component | Verification Test | Acceptance Criterion | Result |
|-----------|------------------|---------------------|--------|
| SceneManager | MBCA-VER-001 | Base layer never written by ResultsApplicator | SKIP (requires USD) |
| SceneManager | MBCA-VER-002 | Results layer never persisted to disk | SKIP (requires USD) |
| ResultsApplicator | MBCA-VER-003 | Position values map exactly to xformOp:translate | SKIP (requires USD) |
| ResultsApplicator | MBCA-VER-004 | Warning logged for unknown prim paths | SKIP (requires USD) |
| MicrobotaModule | MBCA-VER-005 | set_attribute() writes to override layer only | PASS |
| StubPhysicsProcess | MBCA-VER-006 | simTime monotonically increasing, no NaN/Inf | PASS |

> **Note**: MBCA-VER-001 through MBCA-VER-004 require OpenUSD C++ libraries.
> The test code is written and will pass when USD is installed. The tests
> currently emit WARN and skip gracefully.
