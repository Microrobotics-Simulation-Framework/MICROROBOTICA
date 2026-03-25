#include "experiment/experiment_loader.h"
#include <fstream>
#include <spdlog/spdlog.h>

namespace microbotica::experiment {

ExperimentLoader::ExperimentLoader() {
    MBCA_EXPERIMENTAL_WARN("ExperimentLoader");
}

std::optional<ExperimentConfig> ExperimentLoader::load(const std::string& experiment_dir) const {
    std::string json_path = experiment_dir + "/experiment.json";
    return loadFromJson(json_path);
}

std::optional<ExperimentConfig> ExperimentLoader::loadFromJson(const std::string& json_path) const {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        spdlog::error("ExperimentLoader: Cannot open {}", json_path);
        return std::nullopt;
    }

    nlohmann::json j;
    try {
        file >> j;
    } catch (const nlohmann::json::parse_error& e) {
        spdlog::error("ExperimentLoader: JSON parse error in {}: {}", json_path, e.what());
        return std::nullopt;
    }

    return parseConfig(j, json_path);
}

bool ExperimentLoader::isRelativePath(const std::string& path) const {
    if (path.empty()) return true;
    return path[0] != '/';
}

std::optional<ExperimentConfig> ExperimentLoader::parseConfig(
    const nlohmann::json& j,
    const std::string& source_path) const
{
    ExperimentConfig config;

    // Schema version (required)
    if (!j.contains("schema_version")) {
        spdlog::error("ExperimentLoader: Missing schema_version in {}", source_path);
        return std::nullopt;
    }
    config.schema_version = j["schema_version"].get<std::string>();

    // Validate major version == 1
    if (config.schema_version.empty() || config.schema_version[0] != '1') {
        spdlog::error("ExperimentLoader: Unsupported schema_version '{}' in {} (expected 1.x)",
                       config.schema_version, source_path);
        return std::nullopt;
    }

    // mime_version_min (optional, top-level)
    if (j.contains("mime_version_min")) {
        config.mime_version_min = j["mime_version_min"].get<std::string>();
    }

    // Physics section (required)
    if (!j.contains("physics")) {
        spdlog::error("ExperimentLoader: Missing physics section in {}", source_path);
        return std::nullopt;
    }
    const auto& physics = j["physics"];
    config.physics_setup = physics.value("setup", "");
    config.physics_params = physics.value("params", "");

    if (!isRelativePath(config.physics_setup)) {
        spdlog::error("ExperimentLoader: Absolute path rejected: physics.setup = '{}'",
                       config.physics_setup);
        return std::nullopt;
    }
    if (!isRelativePath(config.physics_params)) {
        spdlog::error("ExperimentLoader: Absolute path rejected: physics.params = '{}'",
                       config.physics_params);
        return std::nullopt;
    }

    // Control section (optional)
    if (j.contains("control")) {
        config.controller = j["control"].value("controller", "");
        if (!isRelativePath(config.controller)) {
            spdlog::error("ExperimentLoader: Absolute path rejected: control.controller = '{}'",
                           config.controller);
            return std::nullopt;
        }
    }

    // Scene section (required)
    if (!j.contains("scene")) {
        spdlog::error("ExperimentLoader: Missing scene section in {}", source_path);
        return std::nullopt;
    }
    const auto& scene = j["scene"];
    config.scene_world = scene.value("world", "");
    if (!isRelativePath(config.scene_world)) {
        spdlog::error("ExperimentLoader: Absolute path rejected: scene.world = '{}'",
                       config.scene_world);
        return std::nullopt;
    }

    // Scene actors
    if (scene.contains("actors") && scene["actors"].is_object()) {
        for (auto& [name, actor_json] : scene["actors"].items()) {
            ActorConfig ac;
            ac.prim_path = actor_json.value("prim_path", "");
            ac.prim_type = actor_json.value("prim_type", "Xform");
            if (actor_json.contains("state_fields") && actor_json["state_fields"].is_array()) {
                for (const auto& sf : actor_json["state_fields"]) {
                    ac.state_fields.push_back(sf.get<std::string>());
                }
            }
            config.actors[name] = std::move(ac);
        }
    }

    // Scene analysis
    if (scene.contains("analysis") && scene["analysis"].is_object()) {
        for (auto& [name, an_json] : scene["analysis"].items()) {
            AnalysisConfig ac;
            ac.prim_path = an_json.value("prim_path", "");
            ac.type = an_json.value("type", "");
            if (an_json.contains("resolution") && an_json["resolution"].is_array()) {
                for (const auto& r : an_json["resolution"]) {
                    ac.resolution.push_back(r.get<int>());
                }
            }
            config.analysis[name] = std::move(ac);
        }
    }

    // Cloud section (optional)
    if (j.contains("cloud") && j["cloud"].is_object()) {
        CloudConfig cc;
        cc.job_config = j["cloud"].value("job_config", "");
        cc.cluster_name = j["cloud"].value("cluster_name", "");
        if (!isRelativePath(cc.job_config)) {
            spdlog::error("ExperimentLoader: Absolute path rejected: cloud.job_config = '{}'",
                           cc.job_config);
            return std::nullopt;
        }
        config.cloud = std::move(cc);
    }

    spdlog::info("ExperimentLoader: Loaded experiment (schema {}, {} actors, {} analysis prims)",
                 config.schema_version, config.actors.size(), config.analysis.size());
    return config;
}

} // namespace microbotica::experiment
