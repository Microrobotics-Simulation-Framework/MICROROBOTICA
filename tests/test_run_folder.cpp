// A MIME 0.3 run folder read for playback, plots and claims: the npz reader against NumPy's own files (stored, as MIME
// writes outputs.npz, and deflated), and the outputs of a recorded A0 run (tests/fixtures/runs/a0_recorded: its
// outputs.json, outputs.npz and run.json, from run_to(..., record=["swim_speed:umr.speed"], every=ms(16.5))).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/verification_registry.h"
#include "runs/npz.h"
#include "runs/run_folder.h"
#include "test_config.h"

#include <algorithm>

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
