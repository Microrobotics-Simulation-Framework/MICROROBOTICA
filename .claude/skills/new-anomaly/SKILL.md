# New Anomaly Skill — MICROBOTICA

This skill guides the agent through adding a new entry to `docs/validation/known_anomalies.yaml`. Follow every step — the anomaly registry is a Notified Body-facing compliance artifact.

## Trigger

When the user reports a bug, discovers a limitation, finds a memory safety issue, or invokes `/new-anomaly`.

---

## Step 0 — Check if this needs an anomaly entry

**Add an anomaly entry if:**
- A memory safety tool (ASan, UBSan, TSan, Valgrind) found a defect that is not immediately fixed in this commit
- A known limitation produces incorrect or missing output without raising an error (silent failure)
- A behaviour that could mislead a user or downstream system is being documented rather than fixed now
- A bug is being fixed — the anomaly entry records it was open and is now resolved

**Do NOT add an anomaly entry for:**
- Bugs fixed in the same commit they are discovered (fix it, note it in the commit message, done)
- Development stubs that are clearly temporary and not safety-relevant
- Feature gaps that have not yet been implemented (use ROADMAP.md instead)

**Assess the upstream vs. MICROBOTICA scope:**
`MBCA-ANO-*` entries document defects in MICROBOTICA's own C++ code OR MICROBOTICA-specific manifestations of upstream defects. If an upstream MADDENING/MIME anomaly surfaces through MICROBOTICA in a way that MICROBOTICA fails to detect or report, that failure-to-detect is a MICROBOTICA anomaly. The upstream anomaly itself belongs in MADDENING/MIME's registry — cross-reference it by ID string in `safety_relevance_rationale`.

---

## Step 1 — Assign the next ID

Look at the last `anomaly_id` in `docs/validation/known_anomalies.yaml` and increment. IDs are strictly sequential and never reused — even after an anomaly is marked `fixed` or `wont_fix`, its ID is retired.

Format: `MBCA-ANO-XXX` where XXX is zero-padded to three digits (001, 002, ..., 010, 011, ...).

---

## Step 2 — Classify severity

| Severity | When to use |
|---|---|
| `"critical"` | Produces incorrect results with no available workaround |
| `"major"` | Produces incorrect results but a workaround exists |
| `"minor"` | Cosmetic, inconvenience, or quality-of-life issue with no correctness impact |
| `"enhancement"` | Not a defect — a missing feature that could improve the product |

If uncertain between `major` and `critical`: if a user can avoid the defect by doing something differently, it is `major`. If there is no way to avoid incorrect output given a valid usage, it is `critical`.

---

## Step 3 — Assess safety relevance

This is the most important field. Be specific and honest.

| Value | When to use |
|---|---|
| `"safety_relevant"` | The defect can affect output in a way that is inherently safety-critical regardless of context |
| `"not_safety_relevant"` | The defect provably cannot affect any safety-critical output path |
| `"context_dependent"` | Whether it is safety-relevant depends on how the downstream manufacturer deploys MICROBOTICA |

**When in doubt, use `"context_dependent"`.** It is safer to over-flag than to under-flag. The downstream manufacturer's risk management process will make the final determination.

The `safety_relevance_rationale` must explain the reasoning in three parts:
1. What the defect does in a research context (usually benign)
2. What the defect could do in a clinical context (often more serious)
3. What the downstream manufacturer must assess or implement as a control

Memory-safety-relevant defects (`memory_safety_relevant: true`) must always have an explicit `safety_relevance` value — they may not be left at the default.

---

## Step 4 — Write the YAML entry

Append to the `anomalies:` list in `docs/validation/known_anomalies.yaml`:

```yaml
  - anomaly_id: "MBCA-ANO-XXX"
    title: "Short, specific, searchable title — name the component and the failure mode"
    description: >
      Detailed description of what happens. Include:
      - Which component is affected and how
      - Under what conditions the defect occurs
      - What the observable (incorrect) behaviour is
      - What the expected (correct) behaviour would be
    affected_components: ["ComponentName"]   # C++ class name(s), matches ComponentMeta
    affected_versions: "0.1.0 – current"
    severity: "major"                        # critical | major | minor | enhancement
    safety_relevance: "context_dependent"    # safety_relevant | not_safety_relevant | context_dependent
    safety_relevance_rationale: >
      In a research context, [describe impact — usually minor].
      In a clinical context, [describe what could go wrong and why it matters for patient safety].
      The downstream manufacturer must [describe what assessment or control is needed].
    workaround: "Specific actionable workaround, or 'None available' if critical"
    resolution_status: "open"               # open | workaround | fixed | wont_fix | deferred
    resolution_verification: null           # MBCA-VER-XXX when a verification test proves the fix
    detected_by: "manual_review"            # asan | ubsan | tsan | valgrind | manual_review | ci_test | user_report
    memory_safety_relevant: false           # true if UB, memory corruption, or data race involved
    github_issue: null                      # GitHub issue number when created
    date_reported: "YYYY-MM-DD"             # today's date
```

**Pitfalls to avoid:**
- `title` must be specific enough to be searchable. "ResultsApplicator silently drops unknown prim paths" is good. "Bug in scene manager" is not.
- `affected_components` must use exact C++ class names as they appear in `ComponentMeta::component_id` descriptions
- `safety_relevance_rationale` must never be a single sentence. The research/clinical/manufacturer structure is required.
- `resolution_verification` stays `null` until a `MBCA-VER-*` benchmark is written that proves the fix

---

## Step 5 — Update CHANGELOG.md

Under `## [Unreleased]` → `### Known Anomalies`, add:

```markdown
- `MBCA-ANO-XXX`: <one-line summary of the anomaly>
```

---

## Step 6 — Consider creating a GitHub issue

If `detected_by` is not `"manual_review"` (i.e. a tool found it), or if the anomaly is `"major"` or `"critical"`, create a GitHub issue using the `.github/ISSUE_TEMPLATE/anomaly.md` template. Add the issue number to `github_issue` in the YAML entry.

Apply the appropriate labels: `known-anomaly` always; `anomaly:critical`/`anomaly:major`/`anomaly:minor`; `safety-relevant` if applicable.

---

## Step 7 — Run compliance check

```bash
python scripts/check_anomalies.py
```

Must exit 0. Fix any schema errors before committing.

---

## Updating an existing anomaly (when a fix lands)

When a fix is committed for an open anomaly:

1. Change `resolution_status` from `"open"` to `"fixed"`
2. Set `resolution_verification` to the `MBCA-VER-*` ID of the verification test that proves the fix (create one if it doesn't exist — see new-verification-test skill)
3. Add a CHANGELOG entry under `### Fixed`: `MBCA-ANO-XXX fixed: <brief description>. Verified by MBCA-VER-XXX.`
4. Update the SOUP package verification traceability table in `docs/validation/soup_package.md` — the MBCA-VER-* benchmark should appear with status `PASS`
5. Run `python scripts/check_anomalies.py` — it will now validate that the `resolution_verification` ID exists in the benchmark registry

**Never delete an anomaly entry, even after it is fixed.** The historical record is a compliance artifact.