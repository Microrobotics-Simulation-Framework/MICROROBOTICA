#pragma once

#include <string>
#include <vector>
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

/// A quaternion [w, x, y, z] matching USD GfQuatd and MIME convention.
struct Quatf {
    double w = 1.0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Quatf, w, x, y, z)
};

/// Per-prim mesh data updated each frame (vertex colors only; topology is static).
struct MeshData {
    std::vector<Vec3f> vertexColors;  // per-vertex RGB [0,1]

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(MeshData, vertexColors)
};

/// A single frame of simulation results from the physics backend.
///
/// Contains timestamped position, orientation, scalar, and mesh data
/// keyed by actor name. This is the primary data type flowing from
/// PhysicsProcess -> ResultsApplicator -> USD.
struct ResultFrame {
    double simTime = 0.0;
    std::unordered_map<std::string, Vec3f> positions;
    std::unordered_map<std::string, Quatf> orientations;
    std::unordered_map<std::string, double> scalars;
    std::unordered_map<std::string, MeshData> meshes;
};

// Custom to_json/from_json for backward-compatible serialization.
// New fields (orientations, meshes) are optional on deserialization —
// old frames without them produce empty maps.
inline void to_json(nlohmann::json& j, const ResultFrame& f) {
    j = nlohmann::json{
        {"simTime", f.simTime},
        {"positions", f.positions},
        {"orientations", f.orientations},
        {"scalars", f.scalars},
        {"meshes", f.meshes}
    };
}

inline void from_json(const nlohmann::json& j, ResultFrame& f) {
    j.at("simTime").get_to(f.simTime);
    j.at("positions").get_to(f.positions);
    j.at("scalars").get_to(f.scalars);
    if (j.contains("orientations")) {
        j.at("orientations").get_to(f.orientations);
    }
    if (j.contains("meshes")) {
        j.at("meshes").get_to(f.meshes);
    }
}

} // namespace microbotica::core
