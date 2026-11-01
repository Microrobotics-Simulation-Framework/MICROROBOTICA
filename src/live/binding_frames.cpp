#include "live/binding_frames.h"

#include <cmath>

namespace microbotica::live {

BindingFrames::BindingFrames(std::vector<BindingEntry> binding, const FrameSchema& schema, StageUnits units)
    : binding_(std::move(binding)), units_(std::move(units))
{
    if (!(units_.metersPerUnit > 0.0) || !std::isfinite(units_.metersPerUnit)) {
        units_.metersPerUnit = 1.0;
    }
    for (const auto& entry : binding_) {
        const FieldSpec* spec = schema.find(entry.path());
        const std::size_t width = (entry.kind == "vector") ? 3 : 7;
        const std::size_t row = entry.index.value_or(0);
        if (spec == nullptr || spec->count() < (row + 1) * width) {
            unbound_.push_back(entry.prim + " (" + entry.path() + ")");
        }
    }
    for (const auto& spec : schema.fields) {
        if (spec.count() == 1) {
            scalarPaths_.push_back(spec.path);
        }
    }
}

core::Quatf BindingFrames::turnFromZ(double x, double y, double z) {
    // As MIME's recording layer turns an arrow (viz/_recording.py, _turn), so a live arrow and a recorded one agree.
    const double length = std::sqrt(x * x + y * y + z * z);
    if (!(length > 0.0)) {
        return core::Quatf{1.0, 0.0, 0.0, 0.0};
    }
    const double dx = x / length;
    const double dy = y / length;
    const double dz = z / length;
    const double w = 1.0 + dz;
    if (w < 1e-12) {
        return core::Quatf{0.0, 1.0, 0.0, 0.0};
    }
    const double qx = -dy;
    const double qy = dx;
    const double norm = std::sqrt(w * w + qx * qx + qy * qy);
    return core::Quatf{w / norm, qx / norm, qy / norm, 0.0};
}

core::ResultFrame BindingFrames::toResultFrame(const LiveFrame& frame) const {
    core::ResultFrame out;
    out.simTime = frame.time;
    const double perUnit = units_.metersPerUnit;

    auto rowOf = [&frame](const std::string& path, std::size_t row, std::size_t width) -> const double* {
        const auto it = frame.values.find(path);
        if (it == frame.values.end() || it->second.size() < (row + 1) * width) {
            return nullptr;
        }
        return it->second.data() + row * width;
    };

    for (const auto& entry : binding_) {
        if (entry.kind == "pose") {
            const double* p = rowOf(entry.path(), entry.index.value_or(0), 7);
            if (p == nullptr) {
                continue;
            }
            out.positions[entry.prim] = core::Vec3f{p[0] / perUnit, p[1] / perUnit, p[2] / perUnit};
            out.orientations[entry.prim] = core::Quatf{p[3], p[4], p[5], p[6]};
        } else if (entry.kind == "vector") {
            const double* v = rowOf(entry.path(), entry.index.value_or(0), 3);
            if (v == nullptr) {
                continue;
            }
            if (!entry.atNode.empty()) {
                if (const double* at = rowOf(entry.atPath(), 0, 7)) {
                    out.positions[entry.prim] = core::Vec3f{at[0] / perUnit, at[1] / perUnit, at[2] / perUnit};
                }
            }
            out.orientations[entry.prim] = turnFromZ(v[0], v[1], v[2]);
            const auto scale = units_.vectorMetresPerUnit.find(entry.prim);
            const double metresPerFieldUnit = scale != units_.vectorMetresPerUnit.end() ? scale->second : 1.0;
            const double length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) * metresPerFieldUnit;
            out.scales[entry.prim] = length;
            out.visibility[entry.prim] = true;
        }
    }
    for (const auto& path : scalarPaths_) {
        const auto it = frame.values.find(path);
        if (it != frame.values.end() && !it->second.empty()) {
            out.scalars[path] = it->second.front();
        }
    }
    return out;
}

core::PhysicsConfig BindingFrames::physicsConfig() const {
    core::PhysicsConfig config;
    for (const auto& entry : binding_) {
        config.actorToPrimPath[entry.prim] = entry.prim;
    }
    return config;
}

} // namespace microbotica::live
