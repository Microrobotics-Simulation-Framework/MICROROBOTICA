#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include "experiment/experiment_config.h"
#include "experiment/experiment_loader.h"
#include "test_config.h"

using namespace microbotica::experiment;
using namespace microbotica::core;

// Helper to get fixture path
static std::string experimentFixturePath() {
    return microbotica::test::fixturePath("example_experiment");
}

// ── ExperimentLoader meta ────────────────────────────────────────────────────

TEST_CASE("ExperimentLoader: meta is valid", "[unit][experiment]") {
    const auto& m = ExperimentLoader::interfaceMeta();
    REQUIRE(m.component_id == "MBCA-COMP-060");
    REQUIRE(m.stability == StabilityLevel::Experimental);
}

// ── Loading from fixture ─────────────────────────────────────────────────────

TEST_CASE("ExperimentLoader: loads example experiment", "[unit][experiment]") {
    ExperimentLoader loader;
    auto config = loader.load(experimentFixturePath());
    REQUIRE(config.has_value());

    REQUIRE(config->schema_version == "1.0");
    REQUIRE(config->mime_version_min == "0.2.0");
    REQUIRE(config->physics_setup == "physics/setup.py");
    REQUIRE(config->physics_params == "physics/params.py");
    REQUIRE(config->controller == "control/controller.py");
    REQUIRE(config->scene_world == "scene/world.usda");
}

TEST_CASE("ExperimentLoader: actors parsed correctly", "[unit][experiment]") {
    ExperimentLoader loader;
    auto config = loader.load(experimentFixturePath());
    REQUIRE(config.has_value());

    REQUIRE(config->actors.size() == 2);

    REQUIRE(config->actors.count("rigid_body") == 1);
    REQUIRE(config->actors.at("rigid_body").prim_path == "/World/Actors/UMR");
    REQUIRE(config->actors.at("rigid_body").prim_type == "Capsule");
    REQUIRE(config->actors.at("rigid_body").state_fields.size() == 2);

    REQUIRE(config->actors.count("ext_field") == 1);
    REQUIRE(config->actors.at("ext_field").prim_path == "/World/Actors/FieldArrow");
}

TEST_CASE("ExperimentLoader: analysis parsed correctly", "[unit][experiment]") {
    ExperimentLoader loader;
    auto config = loader.load(experimentFixturePath());
    REQUIRE(config.has_value());

    REQUIRE(config->analysis.size() == 1);
    REQUIRE(config->analysis.count("flow_field") == 1);
    REQUIRE(config->analysis.at("flow_field").prim_path == "/World/Analysis/FlowField");
    REQUIRE(config->analysis.at("flow_field").resolution.size() == 2);
    REQUIRE(config->analysis.at("flow_field").resolution[0] == 64);
}

TEST_CASE("ExperimentLoader: cloud section parsed", "[unit][experiment]") {
    ExperimentLoader loader;
    auto config = loader.load(experimentFixturePath());
    REQUIRE(config.has_value());

    REQUIRE(config->cloud.has_value());
    REQUIRE(config->cloud->job_config == "jobs/h100.yaml");
    REQUIRE(config->cloud->cluster_name == "mime-experiment");
}

TEST_CASE("ExperimentLoader: buildPhysicsConfig populates actorToPrimPath", "[unit][experiment]") {
    ExperimentLoader loader;
    auto config = loader.load(experimentFixturePath());
    REQUIRE(config.has_value());

    auto physics_config = config->buildPhysicsConfig();
    REQUIRE(physics_config.actorToPrimPath.size() == 3); // 2 actors + 1 analysis
    REQUIRE(physics_config.actorToPrimPath.at("rigid_body") == "/World/Actors/UMR");
    REQUIRE(physics_config.actorToPrimPath.at("ext_field") == "/World/Actors/FieldArrow");
    REQUIRE(physics_config.actorToPrimPath.at("flow_field") == "/World/Analysis/FlowField");
}

TEST_CASE("ExperimentLoader: buildConnectionConfig from cloud section", "[unit][experiment]") {
    ExperimentLoader loader;
    auto config = loader.load(experimentFixturePath());
    REQUIRE(config.has_value());

    auto cc = config->buildConnectionConfig();
    REQUIRE(cc.mode == microbotica::connection::ConnectionMode::Cloud);
    REQUIRE(cc.skypilot_cluster == "mime-experiment");
}

// ── Validation tests ─────────────────────────────────────────────────────────

TEST_CASE("ExperimentLoader: rejects absolute paths", "[unit][experiment]") {
    // Create a JSON with an absolute path
    nlohmann::json j = {
        {"schema_version", "1.0"},
        {"physics", {{"setup", "/absolute/path/setup.py"}, {"params", "params.py"}}},
        {"scene", {{"world", "scene/world.usda"}}}
    };

    std::string tmp_path = "/tmp/test_abs_path.json";
    std::ofstream f(tmp_path);
    f << j.dump();
    f.close();

    ExperimentLoader loader;
    auto config = loader.loadFromJson(tmp_path);
    REQUIRE_FALSE(config.has_value());
}

TEST_CASE("ExperimentLoader: rejects unsupported schema version", "[unit][experiment]") {
    nlohmann::json j = {
        {"schema_version", "2.0"},
        {"physics", {{"setup", "setup.py"}, {"params", "params.py"}}},
        {"scene", {{"world", "world.usda"}}}
    };

    std::string tmp_path = "/tmp/test_bad_version.json";
    std::ofstream f(tmp_path);
    f << j.dump();
    f.close();

    ExperimentLoader loader;
    auto config = loader.loadFromJson(tmp_path);
    REQUIRE_FALSE(config.has_value());
}

TEST_CASE("ExperimentLoader: rejects missing physics section", "[unit][experiment]") {
    nlohmann::json j = {
        {"schema_version", "1.0"},
        {"scene", {{"world", "world.usda"}}}
    };

    std::string tmp_path = "/tmp/test_no_physics.json";
    std::ofstream f(tmp_path);
    f << j.dump();
    f.close();

    ExperimentLoader loader;
    auto config = loader.loadFromJson(tmp_path);
    REQUIRE_FALSE(config.has_value());
}

TEST_CASE("ExperimentLoader: handles missing optional sections gracefully", "[unit][experiment]") {
    nlohmann::json j = {
        {"schema_version", "1.0"},
        {"physics", {{"setup", "setup.py"}, {"params", "params.py"}}},
        {"scene", {{"world", "world.usda"}}}
        // No control, cloud, sweep, or metadata sections
    };

    std::string tmp_path = "/tmp/test_minimal.json";
    std::ofstream f(tmp_path);
    f << j.dump();
    f.close();

    ExperimentLoader loader;
    auto config = loader.loadFromJson(tmp_path);
    REQUIRE(config.has_value());
    REQUIRE(config->controller.empty());
    REQUIRE_FALSE(config->cloud.has_value());
    REQUIRE(config->actors.empty());
    REQUIRE(config->analysis.empty());

    // buildConnectionConfig defaults to Local when no cloud section
    auto cc = config->buildConnectionConfig();
    REQUIRE(cc.mode == microbotica::connection::ConnectionMode::Local);
}

TEST_CASE("ExperimentLoader: nonexistent file returns nullopt", "[unit][experiment]") {
    ExperimentLoader loader;
    auto config = loader.loadFromJson("/tmp/nonexistent_experiment.json");
    REQUIRE_FALSE(config.has_value());
}
