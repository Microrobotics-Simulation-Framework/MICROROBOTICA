#pragma once

#include <string>
#include <vector>

namespace microbotica::core {

enum class BenchmarkType {
    DataIntegrity,       ///< Data passes through without corruption
    ContractEnforcement, ///< Interface pre/post conditions hold
    LayerSeparation,     ///< USD layer invariants maintained
    ProtocolFidelity,    ///< IPC data fidelity
    UIBehavior,          ///< UI responds correctly to state changes
    Regression           ///< Regression test against known output
};

struct VerificationBenchmark {
    std::string benchmark_id;  ///< e.g., "MBCA-VER-001"
    std::string component_id;  ///< e.g., "MBCA-COMP-003"
    BenchmarkType type;
    std::string description;
    std::string test_file;     ///< e.g., "tests/verification/test_layer_stack.cpp"
    std::string test_case;     ///< Catch2 test case name
};

/// Global verification benchmark registry.
/// Populated at static init time via REGISTER_VERIFICATION_BENCHMARK.
std::vector<VerificationBenchmark>& benchmarkRegistry();

} // namespace microbotica::core

/// Register a Catch2 test as a verification benchmark.
///
/// @param token     Valid C++ identifier token (no quotes, no dashes)
/// @param id_str    Human-readable benchmark ID string, e.g. "MBCA-VER-001"
/// @param comp_id   Component ID string, e.g. "MBCA-COMP-010"
/// @param type      BenchmarkType enum value
/// @param desc      Description string
/// @param file      Source file path (use __FILE__)
/// @param test_name Catch2 TEST_CASE name string (must match exactly)
#define REGISTER_VERIFICATION_BENCHMARK(token, id_str, comp_id, type, desc, file, test_name) \
    namespace { \
        static const bool _reg_##token = []() { \
            microbotica::core::benchmarkRegistry().push_back({ \
                id_str, comp_id, type, desc, file, test_name}); \
            return true; \
        }(); \
    }
