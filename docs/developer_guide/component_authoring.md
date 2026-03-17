# Component Authoring Guide

## New Component Checklist

When adding a new component to MICROBOTICA, follow the
`.claude/skills/new-component/SKILL.md` procedure:

1. Classify: abstract interface (`src/core/`) or concrete implementation
2. Write header with `ComponentMeta` and Doxygen comments
3. Add to `CMakeLists.txt`
4. Write component guide document
5. Write unit tests and at least one verification test
6. Add anomaly entries for known limitations
7. Run compliance scripts before committing

## ID Convention

| ID Format | Purpose | Directory |
|-----------|---------|-----------|
| `MBCA-COMP-XXX` | Abstract interface | `src/core/` |
| `MBCA-IMPL-XXX` | Concrete implementation | `src/<subsystem>/` |
| `MBCA-VER-XXX` | Verification benchmark | `tests/verification/` |
| `MBCA-ANO-XXX` | Known anomaly | `docs/validation/known_anomalies.yaml` |
