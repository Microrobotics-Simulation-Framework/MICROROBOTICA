#pragma once

#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace microbotica::core {

/// A 3D vector with double precision.
struct Vec3f {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Vec3f, x, y, z)
};

/// A single frame of simulation results from the physics backend.
///
/// Contains timestamped position and scalar data keyed by actor name.
/// This is the primary data type flowing from PhysicsProcess → ResultsApplicator → USD.
struct ResultFrame {
    double simTime = 0.0;
    std::unordered_map<std::string, Vec3f> positions;
    std::unordered_map<std::string, double> scalars;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ResultFrame, simTime, positions, scalars)
};

} // namespace microbotica::core
