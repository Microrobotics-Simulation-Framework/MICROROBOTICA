// mime.live/1 as MICROROBOTICA reads it, against messages captured from a real `mime serve` of A0
// (tests/fixtures/live/README.md): hello, replies, binary and JSON frames, the ready file; and the stage binding's
// frames as the prims' transforms, in the stage's own units.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/verification_registry.h"
#include "live/binding_frames.h"
#include "live/live_protocol.h"
#include "test_config.h"

#include <cmath>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace microbotica::live;

namespace {

std::string fixture(const std::string& name) {
    return microbotica::test::fixturePath("live/" + name);
}

std::vector<std::uint8_t> bytes(const std::string& name) {
    std::ifstream in(fixture(name), std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

nlohmann::json json(const std::string& name) {
    std::ifstream in(fixture(name));
    return nlohmann::json::parse(in);
}

const Hello& a0Hello() {
    static const Hello hello = parseHello(json("hello_a0_callback.json"));
    return hello;
}

/// v rotated by the unit quaternion q = (w, x, y, z).
std::array<double, 3> rotate(const microbotica::core::Quatf& q, std::array<double, 3> v) {
    const double w = q.w, x = q.x, y = q.y, z = q.z;
    const double tx = 2 * (y * v[2] - z * v[1]);
    const double ty = 2 * (z * v[0] - x * v[2]);
    const double tz = 2 * (x * v[1] - y * v[0]);
    return {v[0] + w * tx + (y * tz - z * ty), v[1] + w * ty + (z * tx - x * tz), v[2] + w * tz + (x * ty - y * tx)};
}

} // namespace

REGISTER_VERIFICATION_BENCHMARK(
    MBCA_VER_014,
    "MBCA-VER-014",
    "MBCA-IMPL-013",
    microbotica::core::BenchmarkType::ProtocolFidelity,
    "A mime.live/1 binary frame and the same frame as JSON read to the same values, every field",
    __FILE__,
    "mime.live/1: a binary frame reads as the same frame sent as JSON, value for value"
)

REGISTER_VERIFICATION_BENCHMARK(
    MBCA_VER_015,
    "MBCA-VER-015",
    "MBCA-IMPL-013",
    microbotica::core::BenchmarkType::DataIntegrity,
    "The stage binding puts each frame's poses on their prims in the stage's units",
    __FILE__,
    "Binding frames: poses land on their prims, converted to the stage's units"
)

TEST_CASE("mime.live/1: A0's hello reads whole", "[unit][live]") {
    const Hello& h = a0Hello();
    CHECK(h.protocol == "mime.live/1");
    CHECK(h.experiment == "script_a0");
    CHECK(h.dt == Catch::Approx(0.0005));
    CHECK(h.chunkSteps == 40);
    REQUIRE(h.controller.has_value());
    CHECK(h.controller->entry == "arm_tracking 0.1.0");
    CHECK(h.controller->takesLiveInputs());
    REQUIRE(h.liveInputs.size() == 1);
    CHECK(h.liveInputs[0].label == "offset");
    CHECK(h.liveInputs[0].units == "m");
    CHECK(h.liveInputs[0].shape.empty());
    CHECK_FALSE(h.liveInputs[0].meaning.empty());
    CHECK(h.stage == "/data/a0_callback_saved/scene/scene.usda");
    REQUIRE(h.visuals.has_value());
    CHECK_FALSE(h.visuals->overlay.has_value());
    CHECK(h.visuals->missing.at("ar4_mk5") == 18);
    // The set's notice (credit, licence, terms) is there whether its meshes are drawn or missing.
    const auto notice = h.visuals->noticeLines();
    REQUIRE(notice.size() == 1);
    CHECK(notice[0].find("Annin Robotics") != std::string::npos);
    CHECK(h.binding.size() == 31);
    CHECK(h.schema.id == 2645518914u);
    CHECK(h.schema.frameBytes == 900);
    CHECK(h.schema.fields.size() == 8);
    REQUIRE(h.schema.find("rpm.arm.frame_poses") != nullptr);
    CHECK(h.schema.find("rpm.arm.frame_poses")->shape == std::vector<std::size_t>{27, 7});
    CHECK(h.commands.size() == 9);
    CHECK(h.topology["nodes"].size() == 10);
    CHECK(h.status.state == "ready");
    CHECK(h.status.pace.value_or(0.0) == Catch::Approx(1.0));

    // The binding's vector: the field at the body, drawn at the body's pose.
    const auto vector = std::find_if(h.binding.begin(), h.binding.end(),
                                     [](const BindingEntry& e) { return e.kind == "vector"; });
    REQUIRE(vector != h.binding.end());
    CHECK(vector->prim == "/World/rpm/magnetic/magnetic_torque_umr");
    CHECK(vector->path() == "rpm.magnetic.flux_density_magnetic_torque_umr");
    CHECK(vector->atPath() == "umr.pose");
}

TEST_CASE("mime.live/1: a binary frame reads as the same frame sent as JSON, value for value", "[unit][live]") {
    const Hello& h = a0Hello();
    const auto binary = bytes("frame_1.bin");
    const LiveFrame b = decodeFrame(binary.data(), binary.size(), h.schema);
    CHECK(b.number == 1);
    CHECK(b.step == 40);
    CHECK(b.time == Catch::Approx(0.02));
    CHECK_FALSE(b.final);

    const auto text = bytes("frame_1.json");
    const LiveFrame j = decodeFrame(text.data(), text.size(), h.schema);
    CHECK(j.number == b.number);
    CHECK(j.step == b.step);
    REQUIRE(b.values.size() == h.schema.fields.size());
    for (const auto& spec : h.schema.fields) {
        INFO(spec.path);
        REQUIRE(b.values.at(spec.path).size() == spec.count());
        CHECK(b.values.at(spec.path) == j.values.at(spec.path));  // float32 widened: exact
    }
    // The body starts 1 mm below the tube's axis, its pose's quaternion a unit one.
    const auto& pose = b.values.at("umr.pose");
    CHECK(pose[2] == Catch::Approx(-0.001).margin(1e-5));
    CHECK(pose[3] * pose[3] + pose[4] * pose[4] + pose[5] * pose[5] + pose[6] * pose[6] ==
          Catch::Approx(1.0).margin(1e-5));
}

TEST_CASE("mime.live/1: the session's last frame carries the final flag; frame 0 is the starting state",
          "[unit][live]") {
    const Hello& h = a0Hello();
    const auto last = bytes("frame_last.bin");
    CHECK(decodeBinaryFrame(last.data(), last.size(), h.schema).final);
    const auto first = bytes("frame_0.bin");
    const LiveFrame f0 = decodeBinaryFrame(first.data(), first.size(), h.schema);
    CHECK(f0.number == 0);
    CHECK(f0.step == 0);
    CHECK(f0.time == 0.0);
}

TEST_CASE("mime.live/1: a frame of another schema, length or magic is refused", "[unit][live]") {
    const Hello& h = a0Hello();
    auto frame = bytes("frame_1.bin");
    {
        auto other = frame;
        other[8] ^= 0xFF;  // the schema id
        CHECK_THROWS_AS(decodeBinaryFrame(other.data(), other.size(), h.schema), ProtocolError);
    }
    {
        auto shorter = frame;
        shorter.pop_back();
        CHECK_THROWS_AS(decodeBinaryFrame(shorter.data(), shorter.size(), h.schema), ProtocolError);
    }
    {
        auto version = frame;
        version[4] = 2;  // version 2
        CHECK_THROWS_AS(decodeBinaryFrame(version.data(), version.size(), h.schema), ProtocolError);
    }
    const std::string notFrame = "not a frame";
    CHECK_THROWS_AS(decodeFrame(reinterpret_cast<const std::uint8_t*>(notFrame.data()), notFrame.size(), h.schema),
                    ProtocolError);
}

TEST_CASE("mime.live/1: replies, accepted and refused, keep their status and MIME's coded errors", "[unit][live]") {
    const auto replies = json("replies_a0_callback.json");
    const Reply step = parseReply(replies["step"]);
    CHECK(step.ok);
    CHECK(step.command == "step");
    CHECK(step.status.state == "paused");
    CHECK(step.status.step == 120);
    CHECK(step.status.frames == 4);
    CHECK(step.status.stepping == 0);  // no step in hand once a step's reply comes

    const Reply refused = parseReply(replies["set_refused"]);
    CHECK_FALSE(refused.ok);
    REQUIRE(refused.errors.size() == 1);
    CHECK(refused.errors[0].code == "live.input-unknown");
    CHECK(refused.errors[0].cls == "user");
    CHECK(refused.errors[0].page.rfind("https://", 0) == 0);

    const Reply unknown = parseReply(replies["unknown_refused"]);
    CHECK_FALSE(unknown.ok);
    CHECK(unknown.errors.at(0).code == "live.command");

    CHECK(parseReply(replies["record_on"]).status.recording);
    CHECK_FALSE(parseReply(replies["pace"]).status.pace.has_value());  // unpaced
    const Reply stop = parseReply(replies["stop"]);
    CHECK(stop.status.state == "stopped");
    CHECK(stop.folder.value_or("") == "/data/runs/script_a0-live");

    CHECK_THROWS_AS(parseReply(std::string("not json")), ProtocolError);
    CHECK_THROWS_AS(parseReply(nlohmann::json{{"protocol", "mime.live/2"}, {"ok", true}}), ProtocolError);
}

TEST_CASE("mime.live/1: requests are JSON objects naming the protocol and the command", "[unit][live]") {
    CHECK(stepRequest(3) == nlohmann::json{{"protocol", "mime.live/1"}, {"command", "step"}, {"chunks", 3}});
    CHECK(setRequest({{"offset", 0.002}}) ==
          nlohmann::json{{"protocol", "mime.live/1"}, {"command", "set"}, {"values", {{"offset", 0.002}}}});
    CHECK(paceRequest(std::nullopt)["rtf"].is_null());
    CHECK(paceRequest(2.0)["rtf"] == 2.0);
    CHECK(recordRequest(true)["on"] == true);
    for (const auto& r : {helloRequest(), statusRequest(), startRequest(), pauseRequest(), stopRequest()}) {
        CHECK(r.is_object());
        CHECK(r["protocol"] == "mime.live/1");
    }
}

TEST_CASE("mime.live/1: the ready file names both endpoints and the run folder", "[unit][live]") {
    std::ifstream in(fixture("ready.json"));
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const ReadyFile ready = parseReadyFile(text);
    CHECK(ready.commands.rfind("tcp://127.0.0.1:", 0) == 0);
    CHECK(ready.frames.rfind("tcp://127.0.0.1:", 0) == 0);
    CHECK(ready.out == "/data/runs/script_a0-live");
    CHECK_THROWS_AS(parseReadyFile(R"({"protocol":"mime.live/1","commands":"tcp://127.0.0.1:1"})"), ProtocolError);
}

TEST_CASE("mime.live/1: frame order says a gap and a frame out of order", "[unit][live]") {
    FrameOrder order;
    CHECK(order.see(0).empty());
    CHECK(order.see(1).empty());
    CHECK(order.see(4) == "frames 2 to 3 were dropped before frame 4");
    CHECK(order.see(3).find("out of order") != std::string::npos);
    CHECK(order.dropped() == 2);
    CHECK(order.outOfOrder() == 1);

    // A client that joins a running session counts from the first frame it sees.
    FrameOrder joined;
    CHECK(joined.see(120).empty());
    CHECK(joined.see(121).empty());
    CHECK(joined.dropped() == 0);
}

TEST_CASE("Binding frames: poses land on their prims, converted to the stage's units", "[unit][live]") {
    const Hello& h = a0Hello();
    const auto data = bytes("frame_1.bin");
    const LiveFrame frame = decodeBinaryFrame(data.data(), data.size(), h.schema);
    const auto& pose = frame.values.at("umr.pose");
    const auto& arm = frame.values.at("rpm.arm.frame_poses");

    StageUnits metres;
    BindingFrames inMetres(h.binding, h.schema, metres);
    CHECK(inMetres.unbound().empty());
    CHECK(inMetres.physicsConfig().actorToPrimPath.size() == 31);
    const auto r = inMetres.toResultFrame(frame);
    CHECK(r.simTime == Catch::Approx(0.02));
    CHECK(r.positions.at("/World/umr").z == pose[2]);
    CHECK(r.orientations.at("/World/umr").w == pose[3]);
    CHECK(r.orientations.at("/World/umr").z == pose[6]);
    // A chain frame: row 1 of the arm's frame poses is base_link.
    CHECK(r.positions.at("/World/rpm/arm/base_link").x == arm[7 + 0]);
    CHECK(r.orientations.at("/World/rpm/arm/base_link").y == arm[7 + 5]);
    // The frame's scalars, by path, for the plots.
    CHECK(r.scalars.count("swim_speed:umr.speed") == 1);
    CHECK(r.scalars.count("drag:umr.tilt_fired") == 1);

    // The same frame on a stage written in millimetres: every position a thousand times larger, orientations as
    // they were. (MIME's frames are in metres whatever the stage's units.)
    StageUnits millimetres;
    millimetres.metersPerUnit = 0.001;
    const auto mm = BindingFrames(h.binding, h.schema, millimetres).toResultFrame(frame);
    CHECK(mm.positions.at("/World/umr").z == Catch::Approx(pose[2] * 1000.0));
    CHECK(mm.positions.at("/World/rpm/arm/base_link").x == Catch::Approx(arm[7] * 1000.0));
    CHECK(mm.orientations.at("/World/umr").w == pose[3]);
}

TEST_CASE("Binding frames: a field vector's arrow at the body, along the field, its length in metres",
          "[unit][live]") {
    const Hello& h = a0Hello();
    const auto data = bytes("frame_1.bin");
    const LiveFrame frame = decodeBinaryFrame(data.data(), data.size(), h.schema);
    const auto& b = frame.values.at("rpm.magnetic.flux_density_magnetic_torque_umr");
    const auto& pose = frame.values.at("umr.pose");
    const std::string arrow = "/World/rpm/magnetic/magnetic_torque_umr";

    StageUnits units;
    units.metersPerUnit = 0.001;
    units.vectorMetresPerUnit[arrow] = 2.5;  // metres of arrow per tesla
    const auto r = BindingFrames(h.binding, h.schema, units).toResultFrame(frame);
    CHECK(r.positions.at(arrow).x == Catch::Approx(pose[0] * 1000.0));
    CHECK(r.visibility.at(arrow));
    const double norm = std::sqrt(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);
    REQUIRE(norm > 0.0);
    CHECK(r.scales.at(arrow) == Catch::Approx(norm * 2.5));
    const auto along = rotate(r.orientations.at(arrow), {0.0, 0.0, 1.0});
    CHECK(along[0] == Catch::Approx(b[0] / norm).margin(1e-9));
    CHECK(along[1] == Catch::Approx(b[1] / norm).margin(1e-9));
    CHECK(along[2] == Catch::Approx(b[2] / norm).margin(1e-9));
}

TEST_CASE("Binding frames: +Z turned onto any direction, the zero vector left alone", "[unit][live]") {
    for (const auto& v : std::vector<std::array<double, 3>>{
             {0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {0, -2, 0}, {0.3, -0.4, 0.5}, {-1e-6, 2e-6, -3e-6}}) {
        const auto q = BindingFrames::turnFromZ(v[0], v[1], v[2]);
        const double n = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        const auto d = rotate(q, {0, 0, 1});
        INFO(v[0] << " " << v[1] << " " << v[2]);
        CHECK(d[0] == Catch::Approx(v[0] / n).margin(1e-9));
        CHECK(d[1] == Catch::Approx(v[1] / n).margin(1e-9));
        CHECK(d[2] == Catch::Approx(v[2] / n).margin(1e-9));
    }
    const auto identity = BindingFrames::turnFromZ(0, 0, 0);
    CHECK(identity.w == 1.0);
}
