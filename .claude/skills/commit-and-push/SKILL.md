# Commit and Push Skill — MICROBOTICA

This skill enforces MICROBOTICA's documentation architecture requirements on every commit and push. Run it before pushing to ensure all regulatory and documentation standards are met.

## Trigger

When the user asks to commit and push, or invokes `/commit-and-push`.

---

## Pre-Commit Checklist

Work through the following sections systematically. Each has a **gate** — if it fails, stop and fix before continuing.

---

### 1. Build and Tests Pass

Build the test binary and run the full suite:

```bash
cmake --preset linux-debug
cmake --build build/debug --target microbotica_tests
cd build/debug && ctest --output-on-failure
```

**Gate**: All tests must pass. If any fail, fix them before continuing.

**Note on MBCA-VER-004**: This verification test is expected to fail until the `ResultsApplicator` warning fix lands. A failing `MBCA-VER-004` is not a blocker — it is a known `open` anomaly (`MBCA-ANO-001`). All other tests must pass.

---

### 1b. Memory Safety (ASan/UBSan)

If the `linux-asan` CMake preset is available, run the sanitizer build locally before pushing:

```bash
cmake --preset linux-asan
cmake --build build/asan --target microbotica_tests
LSAN_OPTIONS=suppressions=lsan.suppressions ./build/asan/tests/microbotica_tests
```

> **Prerequisite**: This step assumes USD and Qt headers are already available at the paths configured in `CMakePresets.json` (the same paths used by the `linux-debug` preset). If the debug build already works on this machine, the ASan build will find the same headers. Do not run this step in a fresh environment where USD/Qt have not been built or installed — use the CI `asan-ubsan` job instead, which pulls the pre-cached Docker image.

**Gate**: No ASan/UBSan errors. CI will also run this, but catching memory errors locally is faster. Any ASan/UBSan error that is not immediately fixed must be entered as an `MBCA-ANO-*` entry with `detected_by: "asan"` or `"ubsan"` and `memory_safety_relevant: true` (see new-anomaly skill).

---

### 2. Compliance Scripts Pass

Run all compliance validation scripts:

```bash
python scripts/check_anomalies.py
python scripts/check_citations.py
python scripts/harvest_component_meta.py   # validates ComponentMeta fields are populated
```

**Gate**: All must exit 0. Fix any errors before continuing.

`check_anomalies.py` enforces:
- All `MBCA-ANO-*` IDs are unique and follow the prefix convention
- Required fields are non-empty (`anomaly_id`, `title`, `description`, `severity`, `safety_relevance`, `safety_relevance_rationale`)
- Anomalies with `resolution_status: "fixed"` have a non-null `resolution_verification` field
- Anomalies with `memory_safety_relevant: true` have an explicit `safety_relevance` value (not omitted)

`check_citations.py` enforces:
- Every `[@Key]` citation in `docs/` resolves to an entry in `docs/bibliography.bib`
- Template files (`_template.md`) are excluded

`harvest_component_meta.py` enforces:
- Every `ComponentMeta` instance in `src/` headers has non-empty `component_id`, `description`, `stability`
- No duplicate `component_id` values across the codebase
- Generates a JSON capability matrix at `build/component_meta.json`

---

### 3. Commit Message Convention

Use the correct prefix based on the nature of the change:

| Prefix | When to use |
|---|---|
| `feat:` | New feature or capability |
| `fix:` | Bug fix |
| `refactor:` | Code restructuring (no behaviour change) |
| `docs:` | Documentation-only changes |
| `test:` | Test additions or changes |
| `perf:` | Performance improvement |
| `verify:` | Verification/validation evidence |
| `break:` | Breaking API change |
| `deprecate:` | Deprecation notice |
| `security:` | Security-relevant change |
| `ui:` | UI/UX change |

The commit message body should explain **why**, not what (the diff shows what). Keep the subject line under 72 characters.

---

### 4. CHANGELOG.md Updated

If the change affects user-visible functionality, update `CHANGELOG.md` under `## [Unreleased]`:

- **Added** — new features, new backends, new capabilities
- **Changed** — changes to existing features or behaviour
- **Deprecated** — features marked for future removal
- **Removed** — features removed
- **Fixed** — bug fixes
- **Verification** — changes to V&V status, new benchmarks, benchmark result changes
- **Security** — security-relevant changes (required by MDCG 2019-16)
- **Known Anomalies** — changes to `known_anomalies.yaml` (required for IEC 62304 SOUP)

**When to skip**: Internal refactors, CI config changes, and developer tooling changes that don't affect external behaviour.

---

### 5. New Component Checks (if applicable)

If the commit adds or modifies a class in `src/core/`, `src/scene/`, `src/simulation/`, `src/viewport/`, or `src/scripting/`, verify:

- [ ] Abstract interface (if new): lives in `src/core/`, no Qt/USD/Python includes
- [ ] `ComponentMeta` attached via `interfaceMeta()` or `meta()` static method
- [ ] `ComponentMeta` has: `component_id` (`MBCA-COMP-*` or `MBCA-IMPL-*`), `stability`, `description`, `preconditions`, `postconditions`, `assumptions`, `limitations`, `hazard_hints`
- [ ] `MBCA_EXPERIMENTAL_WARN("ClassName")` in constructor if `stability = Experimental`
- [ ] Doxygen `///` comments on all public methods with `@pre`, `@post`, `@note`
- [ ] Component guide document in `docs/component_guide/interfaces/` or `docs/component_guide/implementations/` following `_template.md`
- [ ] At least one Catch2 unit test
- [ ] At least one verification test registered via `REGISTER_VERIFICATION_BENCHMARK` (see new-verification-test skill)
- [ ] Any new known limitations entered in `docs/validation/known_anomalies.yaml` (see new-anomaly skill)

---

### 6. New Anomaly Checks (if applicable)

If the commit introduces or discovers a known limitation:

- [ ] Entry added to `docs/validation/known_anomalies.yaml` with all required fields (see new-anomaly skill)
- [ ] `anomaly_id` uses `MBCA-ANO-XXX` format (next sequential ID)
- [ ] `safety_relevance_rationale` is non-empty
- [ ] `detected_by` is set (`"manual_review"`, `"asan"`, `"ubsan"`, `"tsan"`, `"valgrind"`, `"ci_test"`, `"user_report"`)
- [ ] `memory_safety_relevant` is explicitly set to `true` or `false`
- [ ] CHANGELOG updated under `### Known Anomalies`
- [ ] `python scripts/check_anomalies.py` passes

---

### 7. Bibliography / Citation Checks (if applicable)

If the commit modifies any file in `docs/` that uses `[@Key]` citations:

- [ ] All `[@Key]` keys have matching entries in `docs/bibliography.bib`
- [ ] Document YAML frontmatter includes `bibliography: ../../bibliography.bib` (adjust relative path as needed)
- [ ] References section has human-readable inline descriptions alongside `[@Key]` markers
- [ ] `python scripts/check_citations.py` passes

---

### 8. API Stability Checks (if applicable)

If the commit changes a public interface in `src/core/`:

- [ ] `ComponentMeta::stability` level is appropriate for the change
- [ ] If changing a `Stable` interface: change is backward-compatible, or this is a MAJOR version bump
- [ ] If removing or incompatibly changing a `Provisional` interface: deprecation notice added in prior release
- [ ] `microrobotica` Python module changes: complete operation safety table in `docs/component_guide/scripting_api.md` updated
- [ ] CHANGELOG updated under `### Changed` or `### Deprecated`

---

## Execution Steps

After all checks pass:

```bash
# Stage specific files — avoid git add -A
git add <specific files>

git commit -m "prefix: concise description of why

Longer explanation if needed.

Co-Authored-By: Claude <noreply@anthropic.com>"

git push
```

After pushing, note that CI will run `build-test` and `asan-ubsan` on every PR. If CI fails, fix and push again. The `tsan` and `valgrind` jobs run on a nightly schedule — a failing nightly job should be treated as a blocking anomaly.

---

## Quick Reference: What Goes Where

| Change Type | CHANGELOG | Anomaly YAML | Component Guide | Bibliography |
|---|---|---|---|---|
| New interface | Added | If limitations exist | Yes (new doc) | If citing references |
| New implementation | Added | If limitations exist | Yes (new doc) | If citing references |
| Bug fix | Fixed | If it was a known anomaly: update `resolution_status` | Update if contract changed | No |
| New limitation discovered | Known Anomalies | Yes (new entry) | Update Known Limitations section | No |
| New verification test | Verification | If fixing anomaly: update `resolution_verification` | Update Verification Evidence | If citing reference |
| Scripting API change | Changed | If new limitation | Update scripting_api.md | No |
| Memory safety error fixed | Fixed | Update `resolution_status` | No | No |
| Documentation only | No (unless user-facing) | No | If component guide | If adding citations |
| Security fix | Security | If safety-relevant | No | No |
| Deprecation | Deprecated | No | Update stability field | No |