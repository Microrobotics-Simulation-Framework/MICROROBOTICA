# New Component Skill — MICROBOTICA

This skill guides the agent through adding a new architectural component to MICROBOTICA — either a new abstract interface in `src/core/` or a new concrete implementation. Follow the steps in order. Do not skip steps.

## Trigger

When the user asks to add a new interface, backend, panel, or implementation class, or invokes `/new-component`.

---

## Step 0 — Classify the component

Before writing any code, decide:

**Is this a new abstract interface?**
- It belongs in `src/core/`
- It must have zero Qt, USD, or Python includes
- It gets component ID `MBCA-COMP-XXX` (next available in the interface range)
- It represents an integration boundary (e.g. `PhysicsProcess`, `RenderBackend`)

**Is this a new concrete implementation?**
- It belongs in the appropriate `src/` subdirectory (`src/stubs/`, `src/scene/`, `src/viewport/`, `src/simulation/`, `src/scripting/`)
- It gets component ID `MBCA-IMPL-XXX` (separate range from interfaces)
- It implements one or more abstract interfaces from `src/core/`

**Is it both?** (new interface + initial implementation together)
- Write the interface first, then the implementation
- They get separate component IDs and separate `ComponentMeta` instances

Check `docs/developer_guide/testing_standards.md` and `DOCUMENTATION_ARCHITECTURE.md` Section 8.1 for the current ID range before assigning a new ID.

---

## Step 1 — Write the header(s)

### For a new abstract interface (`src/core/`):

```cpp
// src/core/my_interface.h
#pragma once

// ONLY stdlib and component_meta.h — NO Qt, USD, or Python
#include "component_meta.h"
#include "types.h"          // if needed for enums
#include "result_frame.h"   // if needed

class MyInterface {
public:
    /// Return structured metadata for this interface.
    /// Concrete implementations override with their own meta().
    static const microbotica::core::ComponentMeta& interfaceMeta() {
        static const microbotica::core::ComponentMeta meta{
            .component_id      = "MBCA-COMP-XXX",
            .component_version = "1.0.0",
            .stability         = microbotica::core::StabilityLevel::Experimental,
            .description       = "One sentence description.",
            .preconditions     = {"..."},
            .postconditions    = {"..."},
            .assumptions       = {"..."},
            .limitations       = {"..."},
            .hazard_hints      = {
                // Technical conditions for ISO 14971 hazard identification.
                // NOT clinical risk assessments — see DOCUMENTATION_ARCHITECTURE.md §8.7.
                "...",
            },
        };
        return meta;
    }

    virtual ~MyInterface() = default;

    /// @pre <precondition>
    /// @post <postcondition>
    /// @throws std::runtime_error if <condition>
    virtual void someMethod() = 0;
};
```

**Rules for `src/core/` headers:**
- No `#include <QObject>`, `#include <pxr/...>`, `#include <pybind11/...>`
- Use only `std::`, `nlohmann::`, and other `src/core/` headers
- Enforce via the `microbotica_core` CMake target (which has no extra include dirs)
- `nlohmann_json >= 3.11.0` is required for `std::optional` serialization in `ComponentMeta` — this is enforced by a `static_assert` in `component_meta.h`
- If the new interface uses `std::optional` fields in its `ComponentMeta`, they will serialize correctly given the version pin

### For a new concrete implementation:

```cpp
// src/<subsystem>/my_impl.h
#pragma once

// Can include Qt, USD, etc. as needed
#include "core/my_interface.h"
#include "core/stability.h"

class MyImpl : public MyInterface {
public:
    MyImpl();  // MBCA_EXPERIMENTAL_WARN goes here

    static const microbotica::core::ComponentMeta& meta() {
        static const microbotica::core::ComponentMeta meta{
            .component_id  = "MBCA-IMPL-XXX",
            .stability     = microbotica::core::StabilityLevel::Experimental,
            .description   = "One sentence description of this specific implementation.",
            .assumptions   = {"..."},
            .limitations   = {"..."},
            .hazard_hints  = {
                // Implementation-specific hazards only.
                // Cross-reference upstream anomalies: "See MIME-ANO-003 for upstream issue."
                "...",
            },
        };
        return meta;
    }

    void someMethod() override;
};
```

```cpp
// src/<subsystem>/my_impl.cpp
#include "my_impl.h"

MyImpl::MyImpl() {
    MBCA_EXPERIMENTAL_WARN("MyImpl");
    // ... rest of constructor
}
```

---

## Step 2 — Add to CMakeLists.txt

Add the new `.cpp` file(s) to the `microbotica` target's source list in the root `CMakeLists.txt`. If the new file is in `src/core/` (header-only interface), no `.cpp` entry is needed.

---

## Step 3 — Write the component guide document

Create `docs/component_guide/interfaces/my_interface.md` or `docs/component_guide/implementations/my_impl.md` by copying the appropriate `_template.md` and filling in:

- Header path and component ID at the top
- Design Rationale — why this component exists, what decision it enables
- Interface Contract — pre/post conditions and invariants in prose + code snippet
- Implementation Map — table tracing each method to its implementation(s)
- Assumptions and Constraints — numbered list
- Known Limitations and Failure Modes — numbered list (feeds directly into anomaly assessment)
- Hazard Hints — technical conditions for ISO 14971 hazard identification (NOT clinical risk assessments)
- Downstream Dependencies — SOUP components this interface interacts with
- Verification Evidence — link to the verification report and test files
- References — `[@Key]` citations if any (add keys to `docs/bibliography.bib`)

**The Implementation Map is mandatory for IEC 62304 Clause 5.4 traceability.** Do not leave it as a stub.

---

## Step 4 — Write tests

### Unit test (`tests/test_my_impl.cpp`):

```cpp
#include <catch2/catch_test_macros.hpp>
#include "my_impl.h"

TEST_CASE("MyImpl: basic construction", "[unit][my-impl]") {
    MyImpl impl;
    REQUIRE(impl.status() == ProcessStatus::Idle);
}

TEST_CASE("MyImpl: someMethod precondition", "[unit][my-impl]") {
    MyImpl impl;
    // Verify precondition enforcement
    REQUIRE_THROWS_AS(impl.someMethod(), std::runtime_error);
}
```

### Verification test (`tests/verification/test_my_impl.cpp`):

See the **new-verification-test** skill for the full pattern. At minimum, register one benchmark covering the most safety-relevant property of this component:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "core/verification_registry.h"
#include "my_impl.h"

REGISTER_VERIFICATION_BENCHMARK(
    MBCA_VER_XXX,                    // token: C++ identifier (underscores, no dashes)
    "MBCA-VER-XXX",                  // id_str: human-readable ID
    "MBCA-COMP-XXX",                 // component being verified
    microbotica::core::BenchmarkType::ContractEnforcement,
    "Description of what this proves",
    __FILE__,
    "The exact Catch2 TEST_CASE name below"
)

TEST_CASE("The exact Catch2 TEST_CASE name below",
          "[verification][contract-enforcement]") {  // [verification] + BenchmarkType tag
    // ... test implementation ...
}
```

Add the new `MBCA-VER-XXX` ID to the Phase 0 verification benchmark table in `DOCUMENTATION_ARCHITECTURE.md` Section 8.3.

---

## Step 5 — Add anomaly entries for known limitations

For each "Known Limitation" in the component guide, assess whether it needs an anomaly entry:

- **Always add**: limitations that could produce incorrect output without an error (`safety_relevance: "context_dependent"` at minimum)
- **Consider adding**: limitations that block correct use but have clear workarounds
- **Skip**: cosmetic limitations, purely developmental stubs that will be replaced

Use the **new-anomaly** skill to create each entry.

---

## Step 6 — Run the pre-commit checklist

Use the **commit-and-push** skill. All compliance scripts must pass before committing.

---

## Quick checklist summary

```markdown
- [ ] Header written in correct directory (src/core/ or src/<subsystem>/)
- [ ] ComponentMeta attached with all required fields
- [ ] MBCA_EXPERIMENTAL_WARN in constructor (if Experimental stability)
- [ ] Doxygen /// comments on all public methods
- [ ] Added to CMakeLists.txt source list
- [ ] Component guide document created from _template.md
- [ ] Implementation Map table complete
- [ ] Hazard hints filled in (technical conditions only)
- [ ] At least one Catch2 unit test
- [ ] At least one verification test registered via REGISTER_VERIFICATION_BENCHMARK
- [ ] Anomaly entries for known limitations
- [ ] check_anomalies.py passes
- [ ] check_citations.py passes (if bibliography entries added)
- [ ] All tests pass
```