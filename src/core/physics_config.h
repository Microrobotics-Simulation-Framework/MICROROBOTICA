#pragma once

#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace microbotica::core {

/// Configuration for launching a physics process.
///
/// Maps actor names (as used in ResultFrame) to USD prim paths
/// (as used in the scene stage).
struct PhysicsConfig {
    std::unordered_map<std::string, std::string> actorToPrimPath;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(PhysicsConfig, actorToPrimPath)
};

} // namespace microbotica::core
