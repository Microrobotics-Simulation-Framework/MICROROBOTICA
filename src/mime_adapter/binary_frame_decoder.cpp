#include "mime_adapter/binary_frame_decoder.h"

#include <cstring>
#include <stdexcept>

#ifdef MICROBOTICA_HAS_ZSTD
#include <zstd.h>
#endif

namespace microbotica::mime {

namespace {

// The wire format is little-endian (fit-up §8). On a little-endian host the
// float32 payload is byte-identical to native float32, so a single memcpy
// suffices for the whole array; on big-endian hosts we fall back to a
// per-float byte-reassembly loop.
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
constexpr bool kHostIsLittleEndian = true;
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
constexpr bool kHostIsLittleEndian = false;
#else
// Unknown — be safe and use the byte-reassembly path.
constexpr bool kHostIsLittleEndian = false;
#endif

std::uint32_t readU32LE(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0])
         | (static_cast<std::uint32_t>(p[1]) << 8)
         | (static_cast<std::uint32_t>(p[2]) << 16)
         | (static_cast<std::uint32_t>(p[3]) << 24);
}

std::uint64_t readU64LE(const std::uint8_t* p) {
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= static_cast<std::uint64_t>(p[i]) << (8 * i);
    }
    return v;
}

float readF32LE(const std::uint8_t* p) {
    const std::uint32_t bits = readU32LE(p);
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

double readF64LE(const std::uint8_t* p) {
    const std::uint64_t bits = readU64LE(p);
    double d;
    std::memcpy(&d, &bits, sizeof(d));
    return d;
}

bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size()
        && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Decompress (or directly copy) the wire payload into `scratch` as
// total_floats consecutive float32s. Throws on size mismatch or
// unsupported compression.
void payloadToFloats(const std::uint8_t* body, std::size_t body_size,
                     const BinaryStreamSchema& schema,
                     std::vector<float>& scratch) {
    const std::size_t total = static_cast<std::size_t>(schema.total_floats);
    const std::size_t expected_bytes = total * 4u;
    scratch.resize(total);

    const std::string& comp = schema.compression;
    if (comp == "none") {
        if (body_size != expected_bytes) {
            throw std::runtime_error(
                "binary frame payload size mismatch: got "
                + std::to_string(body_size) + " bytes, schema expects "
                + std::to_string(expected_bytes) + " ("
                + std::to_string(schema.total_floats) + " float32s)");
        }
        if constexpr (kHostIsLittleEndian) {
            std::memcpy(scratch.data(), body, expected_bytes);
        } else {
            for (std::size_t i = 0; i < total; ++i) {
                scratch[i] = readF32LE(body + i * 4);
            }
        }
        return;
    }

    if (comp == "zstd") {
#ifdef MICROBOTICA_HAS_ZSTD
        const std::size_t got = ZSTD_decompress(
            scratch.data(), expected_bytes, body, body_size);
        if (ZSTD_isError(got)) {
            throw std::runtime_error(
                std::string("zstd decompression failed: ")
                + ZSTD_getErrorName(got));
        }
        if (got != expected_bytes) {
            throw std::runtime_error(
                "binary frame payload size mismatch after zstd: got "
                + std::to_string(got) + " bytes, schema expects "
                + std::to_string(expected_bytes));
        }
        // ZSTD wrote bytes into scratch's float storage. On big-endian
        // hosts that storage interprets each 4-byte chunk in the wrong
        // order; reswap into place.
        if constexpr (!kHostIsLittleEndian) {
            auto* bytes = reinterpret_cast<std::uint8_t*>(scratch.data());
            std::vector<std::uint8_t> tmp(bytes, bytes + expected_bytes);
            for (std::size_t i = 0; i < total; ++i) {
                scratch[i] = readF32LE(tmp.data() + i * 4);
            }
        }
        return;
#else
        throw std::runtime_error(
            "binary frame uses zstd compression, but MICROROBOTICA was built "
            "without libzstd support — rebuild with libzstd available");
#endif
    }

    if (comp == "zstd+xor") {
        throw std::runtime_error(
            "binary frame uses 'zstd+xor' compression, which is stateful and "
            "unsupported (the runner does not emit it)");
    }
    throw std::runtime_error("unknown binary compression: '" + comp + "'");
}

} // namespace

BinaryStreamSchema parseBinarySchema(const nlohmann::json& schema_json) {
    if (!schema_json.is_object()) {
        throw std::runtime_error("binary schema is not a JSON object");
    }

    BinaryStreamSchema schema;
    schema.total_floats = schema_json.value("total_floats", 0);
    schema.frame_bytes  = schema_json.value("frame_bytes", 0);
    schema.compression  = schema_json.value("compression", std::string("none"));

    const auto fields_it = schema_json.find("fields");
    if (fields_it == schema_json.end() || !fields_it->is_array()) {
        throw std::runtime_error("binary schema missing 'fields' array");
    }
    for (const auto& f : *fields_it) {
        BinaryFieldSchema field;
        field.node   = f.value("node", std::string());
        field.field  = f.value("field", std::string());
        field.offset = f.value("offset", 0);
        field.count  = f.value("count", 0);
        schema.fields.push_back(std::move(field));
    }
    return schema;
}

std::vector<DecodePlanEntry> compileDecodePlan(
    const BinaryStreamSchema& schema) {
    std::vector<DecodePlanEntry> plan;
    plan.reserve(schema.fields.size());
    const std::size_t total = static_cast<std::size_t>(schema.total_floats);

    for (const auto& field : schema.fields) {
        // Validate the offset/count once here so the hot path never has to.
        if (field.offset < 0 || field.count < 0
            || static_cast<std::size_t>(field.offset)
                   + static_cast<std::size_t>(field.count) > total) {
            throw std::runtime_error(
                "binary schema field '" + field.node + "/" + field.field
                + "' is out of range for total_floats="
                + std::to_string(schema.total_floats));
        }

        DecodePlanEntry entry;
        entry.offset = field.offset;
        entry.kind = DecodePlanEntry::Kind::Skip;

        if (field.node == "poses" && field.count == 3
            && endsWith(field.field, ".pos")) {
            entry.kind = DecodePlanEntry::Kind::Position;
            entry.name = field.field.substr(0, field.field.size() - 4);
        } else if (field.node == "poses" && field.count == 4
                   && endsWith(field.field, ".quat")) {
            entry.kind = DecodePlanEntry::Kind::Orientation;
            entry.name = field.field.substr(0, field.field.size() - 5);
        } else if (field.node == "scalars" && field.count == 1) {
            entry.kind = DecodePlanEntry::Kind::Scalar;
            entry.name = field.field;
        }
        // Other shapes (e.g. mesh data, non-3 pos) → Skip — the JSON path
        // never sees mesh data through binary; downstream rendering keeps
        // the last mesh state, exactly as for JSON frames that omit
        // "meshes".
        plan.push_back(std::move(entry));
    }
    return plan;
}

core::ResultFrame decodeBinaryFrame(const std::uint8_t* data,
                                    std::size_t size,
                                    const BinaryStreamSchema& schema,
                                    const std::vector<DecodePlanEntry>& plan,
                                    std::vector<float>& scratch) {
    if (size < 8) {
        throw std::runtime_error(
            "binary frame too short: " + std::to_string(size)
            + " bytes (need at least 8 for the sim_time header)");
    }

    core::ResultFrame frame;
    frame.simTime = readF64LE(data);

    payloadToFloats(data + 8, size - 8, schema, scratch);

    // Hot loop: precompiled plan + tight float lookups — no string ops.
    for (const auto& entry : plan) {
        const float* v = scratch.data() + entry.offset;
        switch (entry.kind) {
            case DecodePlanEntry::Kind::Position:
                frame.positions[entry.name] = core::Vec3f{v[0], v[1], v[2]};
                break;
            case DecodePlanEntry::Kind::Orientation:
                // Binary quat order is [w, x, y, z], matching core::Quatf.
                frame.orientations[entry.name] =
                    core::Quatf{v[0], v[1], v[2], v[3]};
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

core::ResultFrame decodeBinaryFrame(const std::uint8_t* data,
                                    std::size_t size,
                                    const BinaryStreamSchema& schema) {
    // Slow path — compile the plan on every call. Convenient for one-shot
    // use (tests, debugging); production callers should cache the plan and
    // call the 5-arg overload.
    const auto plan = compileDecodePlan(schema);
    std::vector<float> scratch;
    return decodeBinaryFrame(data, size, schema, plan, scratch);
}

bool looksLikeJsonMessage(const std::uint8_t* data, std::size_t size) {
    return size > 0 && data[0] == 0x7B;  // '{'
}

} // namespace microbotica::mime
