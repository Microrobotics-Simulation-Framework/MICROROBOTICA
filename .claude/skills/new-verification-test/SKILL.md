# New Verification Test Skill — MICROBOTICA

This skill guides the agent through adding a new verification test to `tests/verification/`. Verification tests are distinct from unit tests — they are formal compliance artifacts registered in the benchmark registry and linked to ComponentMeta IDs.

## Trigger

When the user asks to add a verification test, or invokes `/new-verification-test`. Also invoked as part of the new-component and new-anomaly skills.

---

## The distinction: unit test vs. verification test

| | Unit test | Verification test |
|---|---|---|
| Location | `tests/test_*.cpp` | `tests/verification/test_*.cpp` |
| Registration | None | `REGISTER_VERIFICATION_BENCHMARK` macro |
| Catch2 tag | `[unit]` | `[verification]` and a category tag |
| Purpose | Test implementation correctness | Prove a safety-relevant property holds |
| Linked to | Nothing formal | A `ComponentMeta` component ID and optionally an anomaly `resolution_verification` |
| CI treatment | Runs on every PR | Runs on every PR; failure is a compliance event |

Write a verification test when the thing being tested is:
- A USD layer separation invariant (base layer never modified, results layer never persisted)
- A data integrity property (output value equals input value with no corruption)
- An interface contract enforcement (pre/postconditions enforced at runtime)
- A regression against a known fixed anomaly (`resolution_verification` target)
- A scripting API safety boundary (scripting cannot corrupt safety-relevant state)

Write a unit test (not a verification test) when testing internal implementation details, error messages, or behaviours that are not safety-relevant.

---

## Step 1 — Assign the benchmark ID and token

Check the Phase 0 verification benchmark table in `DOCUMENTATION_ARCHITECTURE.md` Section 8.3 and the existing files in `tests/verification/` to find the next available `MBCA-VER-XXX` ID.

**Token convention** — the token is a valid C++ identifier derived from the ID:
- `MBCA-VER-007` → token `MBCA_VER_007`
- `MBCA-VER-012` → token `MBCA_VER_012`

Underscores, no dashes, no quotes. The token is used only for ODR-safe static initialisation — it never appears in reports or documentation.

---

## Step 2 — Choose the BenchmarkType

| Type | When to use |
|---|---|
| `DataIntegrity` | Proves output values equal input values without corruption |
| `ContractEnforcement` | Proves interface pre/postconditions are enforced at runtime |
| `LayerSeparation` | Proves USD layer invariants (base never written, results never persisted) |
| `ProtocolFidelity` | Proves IPC data (ZeroMQ) arrives without corruption or reordering |
| `UIBehavior` | Proves UI state transitions happen correctly in response to system events |
| `Regression` | Proves a previously fixed defect does not reoccur |

---

## Step 3 — Write the test file

Create `tests/verification/test_<component>.cpp` if it doesn't exist, or add to the existing file for that component.

**Full template:**

```cpp
// tests/verification/test_<component>.cpp

#include <catch2/catch_test_macros.hpp>
#include "core/verification_registry.h"

// Include the component under test
#include "<component_header>.h"

// ── MBCA-VER-XXX ──────────────────────────────────────────────────────────────

REGISTER_VERIFICATION_BENCHMARK(
    MBCA_VER_XXX,                              // token: C++ identifier
    "MBCA-VER-XXX",                            // id_str: human-readable ID
    "MBCA-COMP-XXX",                           // component ID from ComponentMeta
    microbotica::core::BenchmarkType::DataIntegrity,  // BenchmarkType
    "One sentence describing what this proves",
    __FILE__,
    "Exact name of the TEST_CASE below — must match character-for-character"
)

TEST_CASE("Exact name of the TEST_CASE below — must match character-for-character",
          "[verification][data-integrity]") {  // always [verification] + category tag

    // Arrange
    // ...

    // Act
    // ...

    // Assert — use REQUIRE not CHECK for safety-relevant assertions
    // REQUIRE stops the test on first failure; CHECK continues
    REQUIRE(actualValue == expectedValue);
}
```

**Critical rules:**
- The `test_name` string in `REGISTER_VERIFICATION_BENCHMARK` must be **character-for-character identical** to the `TEST_CASE` name string. If they differ, the registry entry cannot be linked to the test run.
- Always use `REQUIRE` (not `CHECK`) for the primary safety-relevant assertion. `CHECK` continues after failure; `REQUIRE` stops immediately, preventing a partially-executed test from masking a deeper failure.
- The Catch2 tag must always include `[verification]`. Add a second category tag derived from the `BenchmarkType`: `[data-integrity]`, `[contract-enforcement]`, `[layer-separation]`, `[protocol-fidelity]`, `[ui-behavior]`, `[regression]`. These are kebab-case versions of the enum values and are used by CI to run subsets of verification tests (e.g. `./microbotica_tests "[verification][layer-separation]"`).

---

## Step 4 — Add to CMakeLists.txt test sources

The `tests/CMakeLists.txt` should glob `tests/verification/*.cpp` or list files explicitly. Verify the new file will be compiled:

```cmake
# In tests/CMakeLists.txt — if using explicit listing, add the new file:
target_sources(microbotica_tests PRIVATE
    # ... existing files ...
    verification/test_<component>.cpp
)
```

---

## Step 5 — Update the benchmark table in DOCUMENTATION_ARCHITECTURE.md

Add the new benchmark to the Phase 0 Verification Benchmarks table in Section 8.3:

```markdown
| `MBCA-VER-XXX` | `MBCA-COMP-XXX` | DataIntegrity | One sentence description |
```

Also update the SOUP package verification traceability table in Section 12 (`docs/validation/soup_package.md`) with the new benchmark:

```markdown
| ComponentName | MBCA-VER-XXX | Acceptance criterion | PASS or FAIL (expected) |
```

---

## Step 6 — Link to anomaly resolution (if applicable)

If this verification test proves that a previously open anomaly is now fixed:

1. Update the anomaly entry in `docs/validation/known_anomalies.yaml`:
   ```yaml
   resolution_status: "fixed"
   resolution_verification: "MBCA-VER-XXX"
   ```
2. Add to CHANGELOG under `### Fixed`: `MBCA-ANO-XXX fixed: <brief>. Verified by MBCA-VER-XXX.`
3. Run `python scripts/check_anomalies.py` — it will verify the `MBCA-VER-XXX` ID exists in the registry

**Special case — intentionally failing tests:**
If the verification test documents *desired* behaviour that is not yet implemented (the test will fail until a fix lands), do not set `resolution_status: "fixed"`. Leave it as `"open"` and set `resolution_verification` to the benchmark ID. The CI failure from the failing test is expected and acceptable until the fix lands. Document this explicitly in the test file:

```cpp
// NOTE: This test currently FAILS — it documents the desired behaviour
// that will fix MBCA-ANO-001. Expected to fail until ResultsApplicator
// warning logging is implemented. See MBCA-ANO-001.
TEST_CASE("ResultsApplicator logs warning for unknown prim paths",
          "[verification][data-integrity]") {
    // ...
}
```

---

## Step 7 — Run the test to confirm it behaves as expected

```bash
cmake --preset linux-debug
cmake --build build/debug --target microbotica_tests
./build/debug/tests/microbotica_tests "[verification]" --reporter console
```

Confirm: the test either passes (if it's a new positive verification) or fails with a clear, descriptive Catch2 failure message (if it's an intentionally-failing future-behaviour test).

---

## Quick checklist

```markdown
- [ ] Next available MBCA-VER-XXX ID assigned
- [ ] Token derived correctly (dashes → underscores, no quotes)
- [ ] BenchmarkType chosen and justified
- [ ] REGISTER_VERIFICATION_BENCHMARK macro placed before TEST_CASE
- [ ] test_name string is character-for-character identical to TEST_CASE name
- [ ] REQUIRE used for primary safety-relevant assertion
- [ ] [verification] tag present in Catch2 tags
- [ ] File added to CMakeLists.txt (if explicit source listing)
- [ ] Benchmark table in DOCUMENTATION_ARCHITECTURE.md updated
- [ ] If fixing anomaly: resolution_status and resolution_verification updated
- [ ] If intentionally failing: comment explaining why the failure is expected
- [ ] Test runs and behaves as expected
```