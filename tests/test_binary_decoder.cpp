#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "core/result_frame.h"
#include "mime_adapter/binary_frame_decoder.h"

using namespace microbotica::mime;
using namespace microbotica::core;

// ── Byte-packing helpers (little-endian, matching BinaryStateEncoder) ─────────

namespace {

void putF64LE(std::vector<std::uint8_t>& buf, double v) {
    std::uint64_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    for (int i = 0; i < 8; ++i) {
        buf.push_back(static_cast<std::uint8_t>((bits >> (8 * i)) & 0xFF));
    }
}

void putF32LE(std::vector<std::uint8_t>& buf, float v) {
    std::uint32_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    for (int i = 0; i < 4; ++i) {
        buf.push_back(static_cast<std::uint8_t>((bits >> (8 * i)) & 0xFF));
    }
}

// The schema from MIME v0.2 fit-up §8: poses body.pos (3 floats), poses
// body.quat (4 floats), scalars drag_torque_z (1 float) — 8 floats total,
// a 40-byte uncompressed frame.
nlohmann::json exampleSchemaJson() {
    return nlohmann::json{
        {"type", "schema"},
        {"fields", nlohmann::json::array({
            {{"node", "poses"},   {"field", "body.pos"},
             {"shape", {3}}, {"offset", 0}, {"count", 3}},
            {{"node", "poses"},   {"field", "body.quat"},
             {"shape", {4}}, {"offset", 3}, {"count", 4}},
            {{"node", "scalars"}, {"field", "drag_torque_z"},
             {"shape", nlohmann::json::array()}, {"offset", 7}, {"count", 1}},
        })},
        {"total_floats", 8},
        {"frame_bytes", 40},
        {"compression", "none"},
    };
}

// Return a double whose little-endian byte 0 is 0x7B ('{'), so a binary
// frame carrying it as sim_time collides with the JSON discriminator.
double makeDoubleStartingWith0x7B(double seed) {
    std::uint64_t bits;
    std::memcpy(&bits, &seed, sizeof(bits));
    bits = (bits & ~static_cast<std::uint64_t>(0xFF)) | 0x7B;
    double d;
    std::memcpy(&d, &bits, sizeof(d));
    return d;
}

// Build a 40-byte uncompressed binary frame for the example schema.
std::vector<std::uint8_t> buildExampleFrame(double sim_time,
                                            float px, float py, float pz,
                                            float qw, float qx, float qy,
                                            float qz, float drag) {
    std::vector<std::uint8_t> buf;
    putF64LE(buf, sim_time);
    putF32LE(buf, px);
    putF32LE(buf, py);
    putF32LE(buf, pz);
    putF32LE(buf, qw);
    putF32LE(buf, qx);
    putF32LE(buf, qy);
    putF32LE(buf, qz);
    putF32LE(buf, drag);
    return buf;
}

} // namespace

// ── Microbenchmark (hidden — run with `microbotica_tests "[.bench]"`) ───────

namespace {

// Build a synthetic schema with `actors` actors × (3-float pos + 4-float
// quat) plus a couple of scalars. Mirrors the ar4 wire layout so the
// per-frame decode cost scales realistically with actor count.
nlohmann::json syntheticSchemaJson(int actors) {
    nlohmann::json fields = nlohmann::json::array();
    int offset = 0;
    for (int a = 0; a < actors; ++a) {
        const std::string name = "actor_" + std::to_string(a);
        fields.push_back({
            {"node", "poses"}, {"field", name + ".pos"},
            {"shape", {3}}, {"offset", offset}, {"count", 3}});
        offset += 3;
        fields.push_back({
            {"node", "poses"}, {"field", name + ".quat"},
            {"shape", {4}}, {"offset", offset}, {"count", 4}});
        offset += 4;
    }
    fields.push_back({
        {"node", "scalars"}, {"field", "drag_torque_z"},
        {"shape", nlohmann::json::array()},
        {"offset", offset}, {"count", 1}});
    offset += 1;
    fields.push_back({
        {"node", "scalars"}, {"field", "sim_progress"},
        {"shape", nlohmann::json::array()},
        {"offset", offset}, {"count", 1}});
    offset += 1;
    return nlohmann::json{
        {"type", "schema"}, {"fields", fields},
        {"total_floats", offset},
        {"frame_bytes", 8 + offset * 4},
        {"compression", "none"},
    };
}

std::vector<std::uint8_t> syntheticFrame(const BinaryStreamSchema& schema) {
    std::vector<std::uint8_t> buf;
    buf.reserve(static_cast<std::size_t>(schema.frame_bytes));
    putF64LE(buf, 1.234567);
    for (int i = 0; i < schema.total_floats; ++i) {
        putF32LE(buf, 0.125f + 0.0625f * static_cast<float>(i));
    }
    return buf;
}

// Bytewise reference decode: replicates the pre-optimisation hot loop —
// cached plan and reused scratch, but the float payload is reassembled
// from bytes via shifts instead of a single memcpy. Used to isolate the
// memcpy contribution from the plan-caching contribution.
inline float readFloatLE(const std::uint8_t* p) {
    const std::uint32_t bits =
        static_cast<std::uint32_t>(p[0])
        | (static_cast<std::uint32_t>(p[1]) << 8)
        | (static_cast<std::uint32_t>(p[2]) << 16)
        | (static_cast<std::uint32_t>(p[3]) << 24);
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

inline double readDoubleLE(const std::uint8_t* p) {
    std::uint64_t bits = 0;
    for (int i = 0; i < 8; ++i) {
        bits |= static_cast<std::uint64_t>(p[i]) << (8 * i);
    }
    double d;
    std::memcpy(&d, &bits, sizeof(d));
    return d;
}

ResultFrame decodeBytewise(const std::uint8_t* data, std::size_t size,
                            const BinaryStreamSchema& schema,
                            const std::vector<DecodePlanEntry>& plan,
                            std::vector<float>& scratch) {
    ResultFrame frame;
    frame.simTime = readDoubleLE(data);
    const std::uint8_t* body = data + 8;
    const std::size_t total = static_cast<std::size_t>(schema.total_floats);
    scratch.resize(total);
    for (std::size_t i = 0; i < total; ++i) {
        scratch[i] = readFloatLE(body + i * 4);
    }
    (void)size;
    for (const auto& entry : plan) {
        const float* v = scratch.data() + entry.offset;
        switch (entry.kind) {
            case DecodePlanEntry::Kind::Position:
                frame.positions[entry.name] = Vec3f{v[0], v[1], v[2]};
                break;
            case DecodePlanEntry::Kind::Orientation:
                frame.orientations[entry.name] = Quatf{v[0], v[1], v[2], v[3]};
                break;
            case DecodePlanEntry::Kind::Scalar:
                frame.scalars[entry.name] = v[0];
                break;
            case DecodePlanEntry::Kind::Skip:
                break;
        }
    }
    return frame;
}

struct BenchResult {
    double slowNs = 0.0;       // recompile plan + heap-alloc scratch each call
    double fastNs = 0.0;       // cached plan + reused scratch + memcpy
    double bytewiseNs = 0.0;   // cached plan + reused scratch, no memcpy
    int totalFloats = 0;
    int frameBytes = 0;
    int iterations = 0;
};

BenchResult runBenchAt(int actorCount, int iterations) {
    const auto schema = parseBinarySchema(syntheticSchemaJson(actorCount));
    const auto frame = syntheticFrame(schema);
    const auto plan = compileDecodePlan(schema);

    std::vector<float> scratch;
    scratch.reserve(static_cast<std::size_t>(schema.total_floats));

    constexpr int kWarmup = 2000;
    for (int i = 0; i < kWarmup; ++i) {
        auto f = decodeBinaryFrame(frame.data(), frame.size(),
                                   schema, plan, scratch);
        (void)f;
    }

    double simSum = 0.0;

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto f = decodeBinaryFrame(frame.data(), frame.size(),
                                   schema, plan, scratch);
        simSum += f.simTime;
    }
    auto t1 = std::chrono::steady_clock::now();
    const double fastNs =
        std::chrono::duration<double, std::nano>(t1 - t0).count() / iterations;

    t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto f = decodeBinaryFrame(frame.data(), frame.size(), schema);
        simSum += f.simTime;
    }
    t1 = std::chrono::steady_clock::now();
    const double slowNs =
        std::chrono::duration<double, std::nano>(t1 - t0).count() / iterations;

    t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto f = decodeBytewise(frame.data(), frame.size(),
                                 schema, plan, scratch);
        simSum += f.simTime;
    }
    t1 = std::chrono::steady_clock::now();
    const double bytewiseNs =
        std::chrono::duration<double, std::nano>(t1 - t0).count() / iterations;

    (void)simSum; // keeps the optimiser honest across the three loops.
    return BenchResult{slowNs, fastNs, bytewiseNs,
                       schema.total_floats,
                       schema.frame_bytes,
                       iterations};
}

} // namespace

TEST_CASE("benchmark: decode hot path — scaling with actor count",
          "[.bench][binary]") {
    struct Row { int actors; int iters; };
    const std::vector<Row> sweeps = {
        {9,    200000},
        {50,   100000},
        {200,   50000},
        {500,   20000},
        {1000,  10000},
    };

    for (const auto& row : sweeps) {
        const auto r = runBenchAt(row.actors, row.iters);
        WARN("BENCH actors=" << row.actors
             << " floats=" << r.totalFloats
             << " bytes=" << r.frameBytes
             << "  slow=" << r.slowNs << "ns"
             << "  fast=" << r.fastNs << "ns"
             << "  bytewise=" << r.bytewiseNs << "ns"
             << "  | speedup=" << (r.slowNs / r.fastNs) << "x"
             << "  | memcpy-vs-bytewise=" << (r.bytewiseNs / r.fastNs) << "x");
    }
}

// ── Schema parsing ───────────────────────────────────────────────────────────

TEST_CASE("parseBinarySchema: parses the fit-up §8 example", "[unit][binary]") {
    auto schema = parseBinarySchema(exampleSchemaJson());

    REQUIRE(schema.total_floats == 8);
    REQUIRE(schema.frame_bytes == 40);
    REQUIRE(schema.compression == "none");
    REQUIRE(schema.valid());
    REQUIRE(schema.fields.size() == 3);

    REQUIRE(schema.fields[0].node == "poses");
    REQUIRE(schema.fields[0].field == "body.pos");
    REQUIRE(schema.fields[0].offset == 0);
    REQUIRE(schema.fields[0].count == 3);

    REQUIRE(schema.fields[1].field == "body.quat");
    REQUIRE(schema.fields[1].offset == 3);
    REQUIRE(schema.fields[1].count == 4);

    REQUIRE(schema.fields[2].node == "scalars");
    REQUIRE(schema.fields[2].field == "drag_torque_z");
    REQUIRE(schema.fields[2].offset == 7);
    REQUIRE(schema.fields[2].count == 1);
}

TEST_CASE("parseBinarySchema: rejects malformed schema", "[unit][binary]") {
    SECTION("not an object") {
        REQUIRE_THROWS(parseBinarySchema(nlohmann::json::array()));
    }
    SECTION("missing fields array") {
        nlohmann::json j = {{"total_floats", 4}, {"compression", "none"}};
        REQUIRE_THROWS(parseBinarySchema(j));
    }
}

TEST_CASE("BinaryStreamSchema::valid: false until populated", "[unit][binary]") {
    BinaryStreamSchema empty;
    REQUIRE_FALSE(empty.valid());
}

// ── Frame decoding round-trip ────────────────────────────────────────────────

TEST_CASE("decodeBinaryFrame: round-trips a known frame", "[unit][binary]") {
    auto schema = parseBinarySchema(exampleSchemaJson());

    // All values are exactly representable in float32, so equality holds.
    // The quaternion uses four distinct values so a wrong w/x/y/z order
    // would be caught.
    auto frame_bytes = buildExampleFrame(
        /*sim_time=*/2.25,
        /*pos=*/0.5f, -1.25f, 3.0f,
        /*quat=*/0.5f, 0.25f, -0.125f, 0.75f,
        /*drag=*/-7.5f);

    REQUIRE(frame_bytes.size() == 40u);

    ResultFrame frame = decodeBinaryFrame(
        frame_bytes.data(), frame_bytes.size(), schema);

    REQUIRE(frame.simTime == 2.25);

    REQUIRE(frame.positions.count("body") == 1);
    REQUIRE(frame.positions.at("body").x == 0.5);
    REQUIRE(frame.positions.at("body").y == -1.25);
    REQUIRE(frame.positions.at("body").z == 3.0);

    REQUIRE(frame.orientations.count("body") == 1);
    REQUIRE(frame.orientations.at("body").w == 0.5);
    REQUIRE(frame.orientations.at("body").x == 0.25);
    REQUIRE(frame.orientations.at("body").y == -0.125);
    REQUIRE(frame.orientations.at("body").z == 0.75);

    REQUIRE(frame.scalars.count("drag_torque_z") == 1);
    REQUIRE(frame.scalars.at("drag_torque_z") == -7.5);

    // Binary mode never carries meshes.
    REQUIRE(frame.meshes.empty());
}

TEST_CASE("decodeBinaryFrame: maps multiple actors", "[unit][binary]") {
    // Two actors, each with pos+quat; ordering matches the encoder's
    // sorted field order (alpha by node, then field).
    nlohmann::json j = {
        {"type", "schema"},
        {"fields", nlohmann::json::array({
            {{"node", "poses"}, {"field", "alpha.pos"},
             {"offset", 0}, {"count", 3}},
            {{"node", "poses"}, {"field", "alpha.quat"},
             {"offset", 3}, {"count", 4}},
            {{"node", "poses"}, {"field", "beta.pos"},
             {"offset", 7}, {"count", 3}},
        })},
        {"total_floats", 10},
        {"frame_bytes", 48},
        {"compression", "none"},
    };
    auto schema = parseBinarySchema(j);

    std::vector<std::uint8_t> buf;
    putF64LE(buf, 1.0);
    putF32LE(buf, 1.0f); putF32LE(buf, 2.0f); putF32LE(buf, 3.0f);   // alpha.pos
    putF32LE(buf, 1.0f); putF32LE(buf, 0.0f);
    putF32LE(buf, 0.0f); putF32LE(buf, 0.0f);                        // alpha.quat
    putF32LE(buf, 4.0f); putF32LE(buf, 5.0f); putF32LE(buf, 6.0f);   // beta.pos

    ResultFrame frame = decodeBinaryFrame(buf.data(), buf.size(), schema);

    REQUIRE(frame.positions.at("alpha").x == 1.0);
    REQUIRE(frame.positions.at("alpha").z == 3.0);
    REQUIRE(frame.orientations.at("alpha").w == 1.0);
    REQUIRE(frame.positions.at("beta").x == 4.0);
    REQUIRE(frame.positions.at("beta").z == 6.0);
    REQUIRE(frame.orientations.count("beta") == 0);
}

// ── Error handling ───────────────────────────────────────────────────────────

TEST_CASE("decodeBinaryFrame: rejects a short frame", "[unit][binary]") {
    auto schema = parseBinarySchema(exampleSchemaJson());
    std::vector<std::uint8_t> tiny = {0x00, 0x01, 0x02, 0x03};
    REQUIRE_THROWS_AS(
        decodeBinaryFrame(tiny.data(), tiny.size(), schema),
        std::runtime_error);
}

TEST_CASE("decodeBinaryFrame: rejects a payload size mismatch", "[unit][binary]") {
    auto schema = parseBinarySchema(exampleSchemaJson()); // expects 8 floats

    std::vector<std::uint8_t> buf;
    putF64LE(buf, 1.0);
    putF32LE(buf, 1.0f); // only one float — schema expects eight
    REQUIRE_THROWS_AS(
        decodeBinaryFrame(buf.data(), buf.size(), schema),
        std::runtime_error);
}

TEST_CASE("decodeBinaryFrame: rejects zstd+xor compression", "[unit][binary]") {
    nlohmann::json j = exampleSchemaJson();
    j["compression"] = "zstd+xor";
    auto schema = parseBinarySchema(j);

    auto buf = buildExampleFrame(1.0, 0, 0, 0, 1, 0, 0, 0, 0);
    REQUIRE_THROWS_AS(
        decodeBinaryFrame(buf.data(), buf.size(), schema),
        std::runtime_error);
}

TEST_CASE("decodeBinaryFrame: rejects unknown compression", "[unit][binary]") {
    nlohmann::json j = exampleSchemaJson();
    j["compression"] = "lz4";
    auto schema = parseBinarySchema(j);

    auto buf = buildExampleFrame(1.0, 0, 0, 0, 1, 0, 0, 0, 0);
    REQUIRE_THROWS_AS(
        decodeBinaryFrame(buf.data(), buf.size(), schema),
        std::runtime_error);
}

// ── JSON / binary message discrimination ─────────────────────────────────────

TEST_CASE("looksLikeJsonMessage: discriminates by leading byte", "[unit][binary]") {
    const std::string json_msg = R"({"type":"params"})";
    REQUIRE(looksLikeJsonMessage(
        reinterpret_cast<const std::uint8_t*>(json_msg.data()),
        json_msg.size()));

    // A binary frame whose sim_time float64 does not begin with 0x7B.
    auto frame = buildExampleFrame(2.25, 0, 0, 0, 1, 0, 0, 0, 0);
    REQUIRE_FALSE(looksLikeJsonMessage(frame.data(), frame.size()));

    // Empty message is not JSON.
    REQUIRE_FALSE(looksLikeJsonMessage(nullptr, 0));
}

TEST_CASE("decodeBinaryFrame: decodes a frame whose sim_time starts with 0x7B",
          "[unit][binary]") {
    auto schema = parseBinarySchema(exampleSchemaJson());

    // sim_time engineered so the frame's first byte is '{' (0x7B) — the
    // same leading byte as a JSON message. ~1/256 of real frames hit this.
    // The decoder is byte-exact and does not depend on the discriminator;
    // the worker resolves the ambiguity by attempting a JSON parse first.
    const double sim_time = makeDoubleStartingWith0x7B(12.5);
    auto frame_bytes = buildExampleFrame(
        sim_time, 1.0f, 2.0f, 3.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    REQUIRE(frame_bytes[0] == 0x7B);
    REQUIRE(looksLikeJsonMessage(frame_bytes.data(), frame_bytes.size()));

    ResultFrame frame = decodeBinaryFrame(
        frame_bytes.data(), frame_bytes.size(), schema);
    REQUIRE(frame.simTime == sim_time);
    REQUIRE(frame.positions.at("body").x == 1.0);
    REQUIRE(frame.positions.at("body").z == 3.0);
}

TEST_CASE("decodeBinaryFrame: rejects a schema field out of range",
          "[unit][binary]") {
    // total_floats=8, but body.quat claims floats [6, 10) — past the end.
    nlohmann::json j = {
        {"type", "schema"},
        {"fields", nlohmann::json::array({
            {{"node","poses"},{"field","body.pos"},{"offset",0},{"count",3}},
            {{"node","poses"},{"field","body.quat"},{"offset",6},{"count",4}},
        })},
        {"total_floats", 8},
        {"frame_bytes", 40},
        {"compression", "none"},
    };
    auto schema = parseBinarySchema(j);
    auto buf = buildExampleFrame(1.0, 0, 0, 0, 1, 0, 0, 0, 0);
    REQUIRE_THROWS_AS(
        decodeBinaryFrame(buf.data(), buf.size(), schema),
        std::runtime_error);
}

// ── End-to-end: handshake + binary stream over ZMQ ───────────────────────────

#ifdef MICROBOTICA_HAS_ZMQ
#include <atomic>
#include <chrono>
#include <thread>
#include <zmq.hpp>

#include "mime_adapter/mime_physics_process.h"

namespace {

/// Mock MIME REP server. Replies {"status":"ok"} to launch/stop and any
/// command, except `stream_info`, which gets the caller-supplied reply.
/// Runs until *running becomes false.
void runMockRepServer(const std::string& endpoint,
                      std::atomic<bool>* running,
                      std::string stream_info_reply) {
    zmq::context_t ctx(1);
    zmq::socket_t rep(ctx, zmq::socket_type::rep);
    rep.set(zmq::sockopt::linger, 0);
    rep.set(zmq::sockopt::rcvtimeo, 300);
    rep.bind(endpoint);

    while (running->load()) {
        zmq::message_t msg;
        if (!rep.recv(msg, zmq::recv_flags::none)) continue;
        const std::string cmd(static_cast<char*>(msg.data()), msg.size());

        std::string reply = R"({"status":"ok"})";
        if (cmd != "launch" && cmd != "stop") {
            try {
                auto j = nlohmann::json::parse(cmd);
                const std::string type =
                    j.value("command", j.value("type", std::string()));
                if (type == "stream_info") reply = stream_info_reply;
            } catch (const std::exception&) {
                // non-JSON command — keep the default ok reply
            }
        }
        zmq::message_t out(reply.size());
        std::memcpy(out.data(), reply.data(), reply.size());
        rep.send(out, zmq::send_flags::none);
    }
}

} // namespace

TEST_CASE("MimePhysicsProcess: handshakes and decodes a binary stream",
          "[integration][binary]") {
    const std::string req_ep = "tcp://127.0.0.1:15565";
    const std::string pub_ep = "tcp://127.0.0.1:15566";

    const nlohmann::json schema = exampleSchemaJson();

    // Mock REP server: acknowledges launch/stop, and answers stream_info.
    // The first stream_info reply omits the schema (encoder built lazily);
    // later replies include it — exercising the re-query path.
    std::atomic<bool> rep_running{true};
    std::atomic<int> stream_info_calls{0};
    std::thread rep_thread([&]() {
        zmq::context_t ctx(1);
        zmq::socket_t rep(ctx, zmq::socket_type::rep);
        rep.set(zmq::sockopt::linger, 0);
        rep.set(zmq::sockopt::rcvtimeo, 500);
        rep.bind(req_ep);

        while (rep_running.load()) {
            zmq::message_t msg;
            auto result = rep.recv(msg, zmq::recv_flags::none);
            if (!result) continue;

            std::string cmd(static_cast<char*>(msg.data()), msg.size());
            std::string reply = R"({"status":"ok"})";

            if (cmd != "launch" && cmd != "stop") {
                try {
                    auto j = nlohmann::json::parse(cmd);
                    std::string type = j.value("command",
                                               j.value("type", std::string()));
                    if (type == "stream_info") {
                        if (++stream_info_calls == 1) {
                            reply = R"({"status":"ok","format":"binary",)"
                                    R"("schema":null})";
                        } else {
                            nlohmann::json r = {
                                {"status", "ok"},
                                {"format", "binary"},
                                {"schema", schema},
                            };
                            reply = r.dump();
                        }
                    }
                } catch (const std::exception&) {
                    // leave default ok reply
                }
            }

            zmq::message_t out(reply.size());
            std::memcpy(out.data(), reply.data(), reply.size());
            rep.send(out, zmq::send_flags::none);
        }
    });

    // Mock PUB server: a one-time JSON params notice, then binary frames.
    std::thread pub_thread([&]() {
        zmq::context_t ctx(1);
        zmq::socket_t pub(ctx, zmq::socket_type::pub);
        pub.set(zmq::sockopt::linger, 0);
        pub.bind(pub_ep);

        std::this_thread::sleep_for(std::chrono::milliseconds(250));

        // Startup params notice (JSON, sent in both stream formats).
        std::string params = R"({"type":"params","data":{"freq":1.0}})";
        zmq::message_t pmsg(params.size());
        std::memcpy(pmsg.data(), params.data(), params.size());
        pub.send(pmsg, zmq::send_flags::none);

        for (int i = 0; i < 8; ++i) {
            auto frame = buildExampleFrame(
                /*sim_time=*/static_cast<double>(i) * 0.5,
                /*pos=*/1.0f * i, 2.0f, 3.0f,
                /*quat=*/1.0f, 0.0f, 0.0f, 0.0f,
                /*drag=*/-0.25f * i);
            zmq::message_t fmsg(frame.size());
            std::memcpy(fmsg.data(), frame.data(), frame.size());
            pub.send(fmsg, zmq::send_flags::none);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    MimePhysicsProcess proc(req_ep, pub_ep);
    PhysicsConfig config;
    config.actorToPrimPath["body"] = "/World/Body";
    proc.launch(config);

    REQUIRE(proc.status() == ProcessStatus::Running);

    int received = 0;
    double last_time = -1.0;
    for (int attempt = 0; attempt < 20 && received < 3; ++attempt) {
        auto frame = proc.receiveResult();
        if (frame && frame->simTime >= 0.0) {
            REQUIRE(frame->simTime >= last_time);
            last_time = frame->simTime;
            REQUIRE(frame->positions.count("body") == 1);
            REQUIRE(frame->positions.at("body").y == 2.0);
            REQUIRE(frame->orientations.at("body").w == 1.0);
            REQUIRE(frame->scalars.count("drag_torque_z") == 1);
            received++;
        }
    }
    REQUIRE(received >= 3);

    // The handshake re-queried stream_info at least twice (null, then schema).
    REQUIRE(stream_info_calls.load() >= 2);

    // Stream stats reflect the binary wire: every frame is 8 + 8*4 = 40 bytes.
    const auto stats = proc.streamStats();
    REQUIRE(stats.format == "binary");
    REQUIRE(stats.compression == "none");
    REQUIRE(stats.framesReceived >= 3);
    REQUIRE(stats.lastFrameBytes == 40);
    REQUIRE(stats.bytesReceived >= 120); // >= 3 frames * 40 bytes
    REQUIRE(stats.decodeErrors == 0);

    proc.stop();
    REQUIRE(proc.status() == ProcessStatus::Stopped);

    rep_running.store(false);
    rep_thread.join();
    pub_thread.join();
}

TEST_CASE("MimePhysicsProcess: handshakes and decodes a JSON stream",
          "[integration][binary]") {
    const std::string req_ep = "tcp://127.0.0.1:15567";
    const std::string pub_ep = "tcp://127.0.0.1:15568";

    // Runner declares JSON — exercises the fallback path end-to-end.
    std::atomic<bool> rep_running{true};
    std::thread rep_thread(
        runMockRepServer, req_ep, &rep_running,
        std::string(R"({"status":"ok","format":"json","schema":null})"));

    std::thread pub_thread([&]() {
        zmq::context_t ctx(1);
        zmq::socket_t pub(ctx, zmq::socket_type::pub);
        pub.set(zmq::sockopt::linger, 0);
        pub.bind(pub_ep);
        std::this_thread::sleep_for(std::chrono::milliseconds(250));

        // One-time startup params notice, then JSON ResultFrames.
        const std::string params = R"({"type":"params","data":{"freq":2.0}})";
        zmq::message_t pmsg(params.size());
        std::memcpy(pmsg.data(), params.data(), params.size());
        pub.send(pmsg, zmq::send_flags::none);

        for (int i = 0; i < 8; ++i) {
            ResultFrame f;
            f.simTime = static_cast<double>(i) * 0.5;
            f.positions["body"] = Vec3f{1.0 * i, 2.0, 3.0};
            f.orientations["body"] = Quatf{1.0, 0.0, 0.0, 0.0};
            const std::string data = nlohmann::json(f).dump();
            zmq::message_t fmsg(data.size());
            std::memcpy(fmsg.data(), data.data(), data.size());
            pub.send(fmsg, zmq::send_flags::none);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    MimePhysicsProcess proc(req_ep, pub_ep);
    PhysicsConfig config;
    config.actorToPrimPath["body"] = "/World/Body";
    proc.launch(config);
    REQUIRE(proc.status() == ProcessStatus::Running);

    int received = 0;
    for (int attempt = 0; attempt < 20 && received < 3; ++attempt) {
        auto frame = proc.receiveResult();
        if (frame && frame->simTime >= 0.0) {
            REQUIRE(frame->positions.at("body").y == 2.0);
            received++;
        }
    }
    REQUIRE(received >= 3);

    const auto stats = proc.streamStats();
    REQUIRE(stats.format == "json");
    REQUIRE(stats.framesReceived >= 3);
    REQUIRE(stats.decodeErrors == 0);

    proc.stop();
    REQUIRE(proc.status() == ProcessStatus::Stopped);

    rep_running.store(false);
    rep_thread.join();
    pub_thread.join();
}

TEST_CASE("MimePhysicsProcess: a binary frame starting with 0x7B is not "
          "mistaken for a JSON message", "[integration][binary]") {
    const std::string req_ep = "tcp://127.0.0.1:15569";
    const std::string pub_ep = "tcp://127.0.0.1:15570";

    const nlohmann::json info = {
        {"status", "ok"}, {"format", "binary"},
        {"schema", exampleSchemaJson()}};

    std::atomic<bool> rep_running{true};
    std::thread rep_thread(runMockRepServer, req_ep, &rep_running, info.dump());

    std::thread pub_thread([&]() {
        zmq::context_t ctx(1);
        zmq::socket_t pub(ctx, zmq::socket_type::pub);
        pub.set(zmq::sockopt::linger, 0);
        pub.bind(pub_ep);
        std::this_thread::sleep_for(std::chrono::milliseconds(250));

        for (int i = 0; i < 8; ++i) {
            // Every frame's sim_time is engineered so the frame begins
            // with 0x7B — the JSON discriminator byte.
            const double st =
                makeDoubleStartingWith0x7B(10.0 + static_cast<double>(i));
            auto frame = buildExampleFrame(
                st, 1.0f * i, 2.0f, 3.0f, 1.0f, 0.0f, 0.0f, 0.0f, -0.5f);
            zmq::message_t fmsg(frame.size());
            std::memcpy(fmsg.data(), frame.data(), frame.size());
            pub.send(fmsg, zmq::send_flags::none);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    MimePhysicsProcess proc(req_ep, pub_ep);
    PhysicsConfig config;
    config.actorToPrimPath["body"] = "/World/Body";
    proc.launch(config);
    REQUIRE(proc.status() == ProcessStatus::Running);

    int received = 0;
    for (int attempt = 0; attempt < 20 && received < 3; ++attempt) {
        auto frame = proc.receiveResult();
        if (frame && frame->simTime >= 0.0) {
            REQUIRE(frame->positions.count("body") == 1);
            received++;
        }
    }
    REQUIRE(received >= 3);

    // The 0x7B frames must decode as binary — failed JSON parses must fall
    // through to the binary decoder, not be counted as decode errors.
    const auto stats = proc.streamStats();
    REQUIRE(stats.format == "binary");
    REQUIRE(stats.framesReceived >= 3);
    REQUIRE(stats.decodeErrors == 0);

    proc.stop();
    rep_running.store(false);
    rep_thread.join();
    pub_thread.join();
}

TEST_CASE("MimePhysicsProcess: a corrupt frame is counted and the stream "
          "keeps flowing", "[integration][binary]") {
    const std::string req_ep = "tcp://127.0.0.1:15571";
    const std::string pub_ep = "tcp://127.0.0.1:15572";

    const nlohmann::json info = {
        {"status", "ok"}, {"format", "binary"},
        {"schema", exampleSchemaJson()}};

    std::atomic<bool> rep_running{true};
    std::thread rep_thread(runMockRepServer, req_ep, &rep_running, info.dump());

    std::thread pub_thread([&]() {
        zmq::context_t ctx(1);
        zmq::socket_t pub(ctx, zmq::socket_type::pub);
        pub.set(zmq::sockopt::linger, 0);
        pub.bind(pub_ep);
        std::this_thread::sleep_for(std::chrono::milliseconds(250));

        // A wrong-sized binary message (8-byte header + 12 bytes = 3 floats;
        // the schema expects 8) — decodeBinaryFrame must reject it.
        const std::vector<std::uint8_t> garbage(20, 0x11);

        for (int i = 0; i < 12; ++i) {
            auto good = buildExampleFrame(
                static_cast<double>(i), 1.0f * i, 2.0f, 3.0f,
                1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
            zmq::message_t g(good.size());
            std::memcpy(g.data(), good.data(), good.size());
            pub.send(g, zmq::send_flags::none);
            std::this_thread::sleep_for(std::chrono::milliseconds(35));

            if (i >= 2 && i % 2 == 0) {
                zmq::message_t bad(garbage.size());
                std::memcpy(bad.data(), garbage.data(), garbage.size());
                pub.send(bad, zmq::send_flags::none);
                std::this_thread::sleep_for(std::chrono::milliseconds(35));
            }
        }
    });

    MimePhysicsProcess proc(req_ep, pub_ep);
    PhysicsConfig config;
    config.actorToPrimPath["body"] = "/World/Body";
    proc.launch(config);
    REQUIRE(proc.status() == ProcessStatus::Running);

    int received = 0;
    for (int attempt = 0; attempt < 40 && received < 6; ++attempt) {
        auto frame = proc.receiveResult();
        if (frame && frame->simTime >= 0.0) received++;
    }
    REQUIRE(received >= 3);

    proc.stop();

    // Good frames flowed; the wrong-sized messages were rejected, not
    // decoded, and did not stall the stream.
    const auto stats = proc.streamStats();
    REQUIRE(stats.framesReceived >= 3);
    REQUIRE(stats.decodeErrors >= 1);

    rep_running.store(false);
    rep_thread.join();
    pub_thread.join();
}
#endif // MICROBOTICA_HAS_ZMQ
