#include "core/verification_registry.h"

namespace microbotica::core {

std::vector<VerificationBenchmark>& benchmarkRegistry()
{
    static std::vector<VerificationBenchmark> registry;
    return registry;
}

} // namespace microbotica::core
