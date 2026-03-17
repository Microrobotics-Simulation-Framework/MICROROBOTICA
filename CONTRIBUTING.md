# Contributing to MICROBOTICA

## Development Environment

### Prerequisites

- **C++ compiler**: GCC 13+ or Clang 16+ with C++17 support
- **CMake**: 3.25+
- **Qt**: 6.4+ (Core, Widgets, OpenGLWidgets)
- **OpenUSD**: 24.08+
- **pybind11**: 2.11+
- **nlohmann_json**: 3.11.0+ (required for `std::optional` serialization)
- **spdlog**: 1.12+
- **Catch2**: 3.4+

### Build

```bash
cmake --preset linux-debug
cmake --build build/debug
```

### Run Tests

```bash
cmake --build build/debug --target microbotica_tests
cd build/debug && ctest --output-on-failure
```

### Run ASan/UBSan

```bash
cmake --preset linux-asan
cmake --build build/asan --target microbotica_tests
LSAN_OPTIONS=suppressions=lsan.suppressions ./build/asan/tests/microbotica_tests
```

## Commit Convention

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

The commit message body should explain **why**, not what (the diff shows what).
Keep the subject line under 72 characters.

## ID Prefix Convention

All MICROBOTICA identifiers use the `MBCA-` prefix:

| ID format | Purpose |
|---|---|
| `MBCA-COMP-XXX` | Abstract interface component IDs |
| `MBCA-IMPL-XXX` | Concrete implementation component IDs |
| `MBCA-ANO-XXX` | Known anomaly entries |
| `MBCA-VER-XXX` | Verification benchmark IDs |

## Adding New Components

See `.claude/skills/new-component/SKILL.md` for the full procedure.

## Adding Anomaly Entries

See `.claude/skills/new-anomaly/SKILL.md` for the full procedure.

## Pre-Commit Checklist

See `.claude/skills/commit-and-push/SKILL.md` for the full checklist.
