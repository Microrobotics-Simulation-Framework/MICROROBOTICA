// MIME's visuals as the viewer says them (src/runs/visuals.*): the command that fetches a folder's meshes, quoted for a
// shell; the saved or run folder a stage is in, found as MIME finds it; `mime visuals view`'s output, read from what
// MIME printed for A0's run folder with an empty asset cache (tests/fixtures/runs/visuals_view_a0_run.txt: MIME's v0.3 on
// 2026-11-04, at a25d8ea before its history rewrite, A0 from scripts/make_a0_fixtures.sh, local paths replaced by
// /data/...); and the credit shown while meshes are drawn.

#include <catch2/catch_test_macros.hpp>

#include "core/verification_registry.h"
#include "runs/run_folder.h"
#include "runs/visuals.h"
#include "test_config.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <unistd.h>

using namespace microbotica::runs;

REGISTER_VERIFICATION_BENCHMARK(
    MBCA_VER_019,
    "MBCA-VER-019",
    "MBCA-COMP-070",
    microbotica::core::BenchmarkType::DataIntegrity,
    "The meshes a run folder lacks are read from mime visuals view's output as MIME printed it, and the command that "
    "fetches them names the folder, quoted for a shell",
    __FILE__,
    "Visuals: mime visuals view's output for A0's run folder, with an empty asset cache"
)

namespace {

std::string fixtureText(const std::string& name) {
    std::ifstream in(microbotica::test::fixturePath(name));
    REQUIRE(in);
    return std::string(std::istreambuf_iterator<char>(in), {});
}

} // namespace

TEST_CASE("Visuals: a folder quoted for a shell", "[unit][runs]") {
    CHECK(shellQuoted("/data/runs/a0_run") == "/data/runs/a0_run");
    CHECK(shellQuoted("runs/a0-run_2.x/") == "runs/a0-run_2.x/");
    CHECK(shellQuoted("/home/me/My Runs/a0") == "'/home/me/My Runs/a0'");
    CHECK(shellQuoted("/data/it's here") == "'/data/it'\\''s here'");
    CHECK(shellQuoted("$HOME/a0") == "'$HOME/a0'");
    CHECK(shellQuoted("~/a0") == "'~/a0'");
    CHECK(shellQuoted("") == "''");
    CHECK(visualsFetchCommand("/data/runs/a0 run") == "mime visuals fetch '/data/runs/a0 run'");
}

TEST_CASE("Visuals: the saved or run folder a stage is in", "[unit][runs]") {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / ("mr_visuals_" + std::to_string(::getpid()));
    fs::remove_all(root);
    fs::create_directories(root / "saved" / "scene");
    std::ofstream(root / "saved" / "mime.lock") << "{}";
    fs::create_directories(root / "run" / "experiment");
    std::ofstream(root / "run" / "experiment" / "mime.lock") << "{}";
    fs::create_directories(root / "loose");

    CHECK(folderOfStage((root / "saved" / "scene" / "scene.usda").string()) == (root / "saved").string());
    CHECK(folderOfStage((root / "run" / "recording.usdc").string()) == (root / "run").string());
    CHECK(folderOfStage((root / "loose" / "stage.usda").string()).empty());
    CHECK(folderOfStage("").empty());
    fs::remove_all(root);
}

TEST_CASE("Visuals: mime visuals view's output for A0's run folder, with an empty asset cache", "[unit][runs]") {
    const VisualsView view = parseVisualsView(fixtureText("runs/visuals_view_a0_run.txt"));
    CHECK(view.layer == "/data/a0_run/recording.usdc");
    CHECK(view.drawn == 0);
    CHECK(view.overlay.empty());
    CHECK(view.missing == std::map<std::string, long>{{"ar4_mk5", 18}});
    REQUIRE_FALSE(view.notice.empty());
    CHECK(view.notice.front().find("Annin Robotics") != std::string::npos);
    CHECK(view.lines.size() == view.notice.size() + 2);  // the count of drawn meshes, the missing set, the notice

    // While drawn, the credit and licence lines stay in view, with what the set is.
    const std::vector<std::string> credit = noticeCredit(view.notice);
    REQUIRE(credit.size() == 3);
    CHECK(credit[0].find("AR4") != std::string::npos);
    CHECK(credit[1].rfind("credit: Annin Robotics", 0) == 0);
    CHECK(credit[2].rfind("licence: ", 0) == 0);
}

TEST_CASE("Visuals: meshes drawn, an output of another shape, and a notice with no credit line", "[unit][runs]") {
    // In the shape MIME's Overlay.text() writes it when the cache draws every mesh (mime.viz, MIME v0.3 at a25d8ea).
    const VisualsView drawn = parseVisualsView(
        "18 meshes drawn from the asset cache (/cache/visuals/overlays/0f.usda)\n"
        "the visual meshes of a robot arm\ncredit: its maker\nlicence: LicenseRef-Its-Makers\nIts terms.\n"
        "/cache/visuals/views/9a.usda\n");
    CHECK(drawn.layer == "/cache/visuals/views/9a.usda");
    CHECK(drawn.drawn == 18);
    CHECK(drawn.overlay == "/cache/visuals/overlays/0f.usda");
    CHECK(drawn.missing.empty());
    CHECK(noticeCredit(drawn.notice) ==
          std::vector<std::string>{"the visual meshes of a robot arm", "credit: its maker", "licence: LicenseRef-Its-Makers"});

    // hello's notice: one text of several lines a set.
    CHECK(noticeCredit({"the visual meshes of a robot arm\ncredit: its maker\nIts terms."}) ==
          std::vector<std::string>{"the visual meshes of a robot arm", "credit: its maker"});
    CHECK(noticeCredit({"A notice of one line."}) == std::vector<std::string>{"A notice of one line."});

    // A wording MIME changes reads as nothing drawn and nothing missing, never as a wrong count.
    const VisualsView other = parseVisualsView("Meshes: 18 drawn\n/data/a0_run/recording.usdc\n");
    CHECK(other.layer == "/data/a0_run/recording.usdc");
    CHECK(other.drawn == 0);
    CHECK(other.missing.empty());
    CHECK(other.notice == std::vector<std::string>{"Meshes: 18 drawn"});
    CHECK(parseVisualsView("").layer.empty());
}
