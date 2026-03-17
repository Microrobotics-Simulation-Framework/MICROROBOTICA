# Verification & Validation

## V&V Philosophy

MICROBOTICA's verification scope is **faithful rendering**: data received
from the physics backend (MIME/MADDENING) must be displayed in the viewport
and accessible via the scripting API without corruption or silent modification.

MICROBOTICA does **not** verify the physical correctness of simulation results.
That responsibility belongs to the upstream physics layers (MADDENING, MIME).

## Verification Domain

| Domain | MICROBOTICA Verifies | Upstream Verifies |
|--------|---------------------|-------------------|
| Position data | Correct display in viewport | Physical accuracy |
| Scalar data | Correct reporting via API | Physical accuracy |
| Scene integrity | Layer separation invariants | N/A |
| Parameter passing | Type safety, range checking | Physical validity |
| Session provenance | Complete audit trail | N/A |

## Artifacts

- [Known Anomalies](known_anomalies.yaml) — anomaly registry
- [SOUP Package](soup_package.md) — IEC 62304 SOUP assessment support
