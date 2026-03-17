#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace microbotica::core {

/// Complete, serializable record of a MICROBOTICA session.
///
/// Captures everything needed to reproduce the session and to serve
/// as an audit trail in a regulatory context.
struct SessionProvenance {
    // Software identity
    std::string microbotica_version;   ///< IEC 62304 Clause 8: SOUP identification
    std::string qt_version;
    std::string usd_version;
    std::string compiler_version;
    std::string platform;

    // Upstream SOUP versions (captured from IPC handshake)
    std::optional<std::string> mime_version;
    std::optional<std::string> maddening_version;

    // Scene identity
    std::string scene_file_path;
    std::string scene_file_hash;       ///< SHA-256 of the loaded USD file

    // Compute backend
    std::string compute_backend_type;  ///< "local" | "skypilot"
    std::optional<std::string> skypilot_job_id;
    std::optional<std::string> cloud_provider;
    std::optional<std::string> container_image_hash;

    // Render backend
    std::string render_backend_type;   ///< "local_hydra" | "selkies_stream"
    std::optional<std::string> selkies_session_id;

    // Simulation parameters
    nlohmann::json initial_parameters;
    std::vector<nlohmann::json> parameter_changes; ///< Timestamped changes

    // Timestamps
    std::string session_started;
    std::string session_ended;

    // Results summary
    int total_frames_received = 0;
    int frames_dropped = 0;
    int out_of_range_warnings = 0;
};

namespace detail {
    inline void opt_to_json(nlohmann::json& j, const char* key,
                            const std::optional<std::string>& v) {
        if (v.has_value()) j[key] = v.value();
        else j[key] = nullptr;
    }
    inline void opt_from_json(const nlohmann::json& j, const char* key,
                              std::optional<std::string>& v) {
        if (j.contains(key) && !j[key].is_null())
            v = j[key].get<std::string>();
        else
            v = std::nullopt;
    }
} // namespace detail

inline void to_json(nlohmann::json& j, const SessionProvenance& p) {
    j = nlohmann::json{
        {"microbotica_version", p.microbotica_version},
        {"qt_version", p.qt_version},
        {"usd_version", p.usd_version},
        {"compiler_version", p.compiler_version},
        {"platform", p.platform},
        {"scene_file_path", p.scene_file_path},
        {"scene_file_hash", p.scene_file_hash},
        {"compute_backend_type", p.compute_backend_type},
        {"render_backend_type", p.render_backend_type},
        {"initial_parameters", p.initial_parameters},
        {"parameter_changes", p.parameter_changes},
        {"session_started", p.session_started},
        {"session_ended", p.session_ended},
        {"total_frames_received", p.total_frames_received},
        {"frames_dropped", p.frames_dropped},
        {"out_of_range_warnings", p.out_of_range_warnings},
    };
    detail::opt_to_json(j, "mime_version", p.mime_version);
    detail::opt_to_json(j, "maddening_version", p.maddening_version);
    detail::opt_to_json(j, "skypilot_job_id", p.skypilot_job_id);
    detail::opt_to_json(j, "cloud_provider", p.cloud_provider);
    detail::opt_to_json(j, "container_image_hash", p.container_image_hash);
    detail::opt_to_json(j, "selkies_session_id", p.selkies_session_id);
}

inline void from_json(const nlohmann::json& j, SessionProvenance& p) {
    j.at("microbotica_version").get_to(p.microbotica_version);
    j.at("qt_version").get_to(p.qt_version);
    j.at("usd_version").get_to(p.usd_version);
    j.at("compiler_version").get_to(p.compiler_version);
    j.at("platform").get_to(p.platform);
    j.at("scene_file_path").get_to(p.scene_file_path);
    j.at("scene_file_hash").get_to(p.scene_file_hash);
    j.at("compute_backend_type").get_to(p.compute_backend_type);
    j.at("render_backend_type").get_to(p.render_backend_type);
    j.at("initial_parameters").get_to(p.initial_parameters);
    j.at("parameter_changes").get_to(p.parameter_changes);
    j.at("session_started").get_to(p.session_started);
    j.at("session_ended").get_to(p.session_ended);
    j.at("total_frames_received").get_to(p.total_frames_received);
    j.at("frames_dropped").get_to(p.frames_dropped);
    j.at("out_of_range_warnings").get_to(p.out_of_range_warnings);
    detail::opt_from_json(j, "mime_version", p.mime_version);
    detail::opt_from_json(j, "maddening_version", p.maddening_version);
    detail::opt_from_json(j, "skypilot_job_id", p.skypilot_job_id);
    detail::opt_from_json(j, "cloud_provider", p.cloud_provider);
    detail::opt_from_json(j, "container_image_hash", p.container_image_hash);
    detail::opt_from_json(j, "selkies_session_id", p.selkies_session_id);
}

} // namespace microbotica::core
