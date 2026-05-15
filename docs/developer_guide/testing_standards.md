# Testing Standards

## Three Test Tiers

### Tier 1: Unit Tests (`tests/test_*.cpp`)

- Test implementation correctness
- Tagged `[unit]`
- No formal registration
- Run on every PR

### Tier 2: Verification Tests (`tests/verification/test_*.cpp`)

- Prove safety-relevant properties
- Tagged `[verification]` plus a category tag
- Registered via `REGISTER_VERIFICATION_BENCHMARK` macro
- Linked to `ComponentMeta` component IDs
- Failure is a compliance event

### Tier 3: Integration Tests

- End-to-end pipeline tests
- Tagged `[integration]`
- Verify multi-component interactions

## Memory Safety

MICROBOTICA uses a four-tool memory safety suite:

### ASan + UBSan (every PR)

```bash
cmake --preset linux-asan
cmake --build build/asan --target microbotica_tests
LSAN_OPTIONS=suppressions=lsan.suppressions ./build/asan/tests/microbotica_tests
```

### TSan (nightly)

```bash
cmake --preset linux-tsan
cmake --build build/tsan --target microbotica_tests
TSAN_OPTIONS=suppressions=tsan.suppressions ./build/tsan/tests/microbotica_tests
```

### Valgrind (weekly)

```bash
valgrind --suppressions=valgrind.suppressions ./build/debug/tests/microbotica_tests
```

### CI Job Structure

| Job | Trigger | Tools |
|-----|---------|-------|
| `build-test` | Every PR | GCC, no sanitizers |
| `asan-ubsan` | Every PR | ASan + UBSan + LSan |
| `tsan` | Nightly | TSan |
| `valgrind` | Weekly | Valgrind/Memcheck |

### Anomaly Reporting

Any ASan/UBSan error that is not immediately fixed must be entered as
an `MBCA-ANO-*` entry with `detected_by: "asan"` or `"ubsan"` and
`memory_safety_relevant: true`. See the new-anomaly skill.

## Suppression Files

- `lsan.suppressions` — CPython/pybind11 shutdown leaks
- `tsan.suppressions` — Qt/Python dispatch false positives
- `valgrind.suppressions` — Qt/Python/{term}`OpenUSD` known leaks
