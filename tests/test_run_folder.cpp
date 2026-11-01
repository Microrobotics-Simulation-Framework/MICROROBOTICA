// A MIME 0.3 run folder read for playback, plots and claims: the npz reader against NumPy's own files (stored, as MIME
// writes outputs.npz, and deflated), and the outputs of a recorded A0 run (tests/fixtures/runs/a0_recorded: its
// outputs.json, outputs.npz, run.json and report.json, from run_to(..., record=["swim_speed:umr.speed"],
// every=ms(16.5))); the build report's summary (mime.report/1) as a run folder and hello give it, and a run folder
// without one or with one of a newer format.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/verification_registry.h"
#include "runs/npz.h"
#include "runs/run_folder.h"
#include "test_config.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unistd.h>

using namespace microbotica::runs;

REGISTER_VERIFICATION_BENCHMARK(
    MBCA_VER_017,
    "MBCA-VER-017",
    "MBCA-COMP-070",
    microbotica::core::BenchmarkType::DataIntegrity,
    "A run folder's recorded series and flags read from outputs.npz as MIME wrote them, at their steps' times",
    __FILE__,
    "Run folder: A0's recorded speed and its flags, at their steps' times"
)

REGISTER_VERIFICATION_BENCHMARK(
    MBCA_VER_018,
    "MBCA-VER-018",
    "MBCA-COMP-070",
    microbotica::core::BenchmarkType::DataIntegrity,
    "The build report's validity lines read as MIME states them (declared ones as not yet checked); a run folder "
    "without a report, or with one of a newer format, still reads, and says why it has none",
    __FILE__,
    "Build report: A0's validity lines, watches and flags as report.json states them"
)

TEST_CASE("npz: NumPy's stored and deflated archives read to the same arrays", "[unit][runs]") {
    for (const char* name : {"npz/stored.npz", "npz/deflated.npz"}) {
        INFO(name);
        const NpzFile npz(microbotica::test::fixturePath(name));
        auto names = npz.names();
        std::sort(names.begin(), names.end());
        CHECK(names == std::vector<std::string>{"final/umr/pose", "flags/f", "scalar", "series/x/steps",
                                                "series/x/values"});
        const NpyArray pose = npz.read("final/umr/pose");
        CHECK(pose.dtype == "<f4");
        CHECK(pose.shape == std::vector<std::size_t>{7});
        CHECK(pose.values[0] == static_cast<double>(0.001f));
        CHECK(pose.values[3] == 1.0);
        const NpyArray values = npz.read("series/x/values");
        CHECK(values.shape == std::vector<std::size_t>{2, 3});
        CHECK(values.values == std::vector<double>{0.0, 0.5, 1.0, 1.5, 2.0, 2.5});
        CHECK(npz.read("series/x/steps").values == std::vector<double>{33, 66});
        CHECK(npz.read("flags/f").values == std::vector<double>{0, 1, 1, 0});
        const NpyArray scalar = npz.read("scalar");
        CHECK(scalar.shape.empty());
        CHECK(scalar.values == std::vector<double>{2.5});
        CHECK_THROWS_AS(npz.read("nothing"), NpzError);
    }
    CHECK_THROWS_AS(NpzFile(microbotica::test::fixturePath("npz/make.py")), NpzError);
}

TEST_CASE("Run folder: A0's recorded speed and its flags, at their steps' times", "[unit][runs]") {
    const std::string folder = microbotica::test::fixturePath("runs/a0_recorded");
    REQUIRE(RunFolder::looksLikeRunFolder(folder));
    CHECK(RunFolder::runFolderOf(folder + "/recording.usdc") == folder);
    CHECK_FALSE(RunFolder::looksLikeRunFolder(microbotica::test::fixturePath("npz")));

    const RunFolder run(folder);
    CHECK(run.experiment() == "script_a0");
    CHECK(run.dt() == Catch::Approx(0.0005));
    CHECK(run.steps() == 3960);
    CHECK(run.duration() == Catch::Approx(1.98));
    CHECK(run.recording().empty());  // run.json names one; this copy of the folder holds none
    CHECK(run.experimentFolder().empty());
    CHECK(run.commandsLog().empty());
    CHECK(run.profile() == "exploratory");
    CHECK(run.pending().size() == 7);
    CHECK_FALSE(run.nonFinite().has_value());

    const auto flags = run.flags();
    REQUIRE(flags.size() == 3);
    const auto tilt = std::find_if(flags.begin(), flags.end(),
                                   [](const RunFlag& f) { return f.path == "drag:umr.tilt_fired"; });
    REQUIRE(tilt != flags.end());
    CHECK(tilt->wasRaised == std::optional<bool>(false));  // recorded every step: known
    CHECK_FALSE(tilt->meaning.empty());

    const auto watches = run.watches();
    REQUIRE(watches.size() == 1);
    CHECK(watches[0].raw.value("status", std::string()) == "above");  // A0's Reynolds number is above Stokes flow's
    CHECK(watches[0].title.find("Reynolds") != std::string::npos);

    const auto series = run.recordedSeries();
    const auto speed = std::find_if(series.begin(), series.end(),
                                    [](const RecordedSeries& s) { return s.path == "swim_speed:umr.speed"; });
    REQUIRE(speed != series.end());
    CHECK(speed->unit == "m/s");
    CHECK_FALSE(speed->flag);
    REQUIRE(speed->values.size() == 120);
    CHECK(speed->times.front() == Catch::Approx(33 * 0.0005));   // the first sample, at step 33
    CHECK(speed->times.back() == Catch::Approx(1.98));
    const auto tiltSeries = std::find_if(series.begin(), series.end(),
                                         [](const RecordedSeries& s) { return s.path == "drag:umr.tilt_fired"; });
    REQUIRE(tiltSeries != series.end());
    CHECK(tiltSeries->flag);
    CHECK(tiltSeries->values.size() == 3960);  // every step
    CHECK(tiltSeries->times.back() == Catch::Approx(1.98));
}

TEST_CASE("Build report: A0's validity lines, watches and flags as report.json states them", "[unit][runs]") {
    const RunFolder run(microbotica::test::fixturePath("runs/a0_recorded"));
    const ReportRead read = run.report();
    CHECK_FALSE(read.unreadable.has_value());
    REQUIRE(read.report.has_value());
    const BuildReport& report = *read.report;
    CHECK(report.experiment == "script_a0");
    REQUIRE_FALSE(report.validity.empty());
    for (const auto& v : report.validity) {
        INFO(v.owner << ": " << v.line);
        CHECK((v.kind == "declared" || v.kind == "computed" || v.kind == "record"));
        CHECK_FALSE(v.line.empty());
        CHECK(v.declaredOnly() == (v.kind == "declared"));
    }
    // What the run was watched against, and the flags it may raise: the same as its outputs.json says of the run.
    CHECK(report.watches.size() == run.watches().size());
    const auto flags = run.flags();
    REQUIRE(report.flags.size() == flags.size());
    for (const auto& f : report.flags) {
        INFO(f.path);
        CHECK(std::any_of(flags.begin(), flags.end(), [&f](const RunFlag& r) { return r.path == f.path; }));
        CHECK_FALSE(f.meaning.empty());
    }
    REQUIRE(report.profile.has_value());
    CHECK(report.profile->profile == run.profile());
    CHECK(report.profile->pending.size() == run.pending().size());
    CHECK_FALSE(report.profile->stamp.empty());
    CHECK_FALSE(report.settings.empty());
    CHECK_FALSE(report.summary.empty());
}

TEST_CASE("Build report: none, one of a newer format, and one that is not JSON", "[unit][runs]") {
    CHECK_FALSE(parseReport(nlohmann::json()).report.has_value());
    CHECK_FALSE(parseReport(nlohmann::json()).unreadable.has_value());
    CHECK(parseReport(nlohmann::json{{"format", "mime.report/2"}}).unreadable.value_or("") == "mime.report/2");
    CHECK(parseReport(nlohmann::json{{"experiment", "x"}}).unreadable.value_or("") == "None");
    CHECK(parseReport(nlohmann::json::array()).unreadable.value_or("") == "not a JSON object");
    const auto minimal = parseReport(nlohmann::json{
        {"format", "mime.report/1"},
        {"validity", {{{"owner", "drag"}, {"kind", "declared"}, {"line", "Re < 1"}}}},
        {"profile", nullptr}});
    REQUIRE(minimal.report.has_value());
    CHECK(minimal.report->validity.at(0).declaredOnly());
    CHECK_FALSE(minimal.report->profile.has_value());

    // A copy of A0's run folder: without report.json (a MIME before reports reached run folders), it reads as before;
    // with one of a newer format, or one that is not JSON, it reads and says why its report was not.
    namespace fs = std::filesystem;
    const fs::path source = microbotica::test::fixturePath("runs/a0_recorded");
    const fs::path copy = fs::temp_directory_path() / ("microrobotica-report-test-" + std::to_string(::getpid()));
    fs::remove_all(copy);
    fs::create_directories(copy);
    for (const char* name : {"outputs.json", "run.json"}) {
        fs::copy_file(source / name, copy / name);
    }
    {
        const RunFolder older(copy.string());
        CHECK(older.reportFile().empty());
        CHECK_FALSE(older.report().report.has_value());
        CHECK_FALSE(older.report().unreadable.has_value());
        CHECK(older.flags().size() == 3);  // the watches-and-flags fallback
        CHECK(older.watches().size() == 1);
    }
    std::ofstream(copy / "report.json") << R"({"format": "mime.report/2", "validity": []})";
    CHECK(RunFolder(copy.string()).report().unreadable.value_or("") == "mime.report/2");
    std::ofstream(copy / "report.json") << "{not json";
    CHECK(RunFolder(copy.string()).report().unreadable.value_or("") == "not JSON");
    CHECK(RunFolder(copy.string()).steps() == 3960);  // the run reads all the same
    fs::remove_all(copy);
}
