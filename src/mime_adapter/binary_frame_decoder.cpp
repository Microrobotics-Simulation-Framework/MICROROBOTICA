#include "mime_adapter/binary_frame_decoder.h"

#include <cstring>
#include <stdexcept>

#ifdef MICROBOTICA_HAS_ZSTD
#include <zstd.h>
#endif

namespace microbotica::mime {

namespace {

// Little-endian readers. The wire format is explicitly little-endian
// (fit-up §8); building each value byte-by-byte keeps the decoder correct
// regardless of host endianness.
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

core::ResultFrame decodeBinaryFrame(const std::uint8_t* data,
                                    std::size_t size,
                                    const BinaryStreamSchema& schema) {
    if (size < 8) {
        throw std::runtime_error(
            "binary frame too short: " + std::to_string(size)
            + " bytes (need at least 8 for the sim_time header)");
    }

    core::ResultFrame frame;
    frame.simTime = readF64LE(data);

    // The 8-byte sim_time header is always plaintext; everything after it
    // is the (possibly compressed) float payload.
    const std::uint8_t* body = data + 8;
    const std::size_t body_size = size - 8;

    std::vector<std::uint8_t> decompressed;
    const std::uint8_t* floats_ptr = body;
    std::size_t floats_size = body_size;

    const std::string& comp = schema.compression;
    if (comp == "none") {
        // Payload is already raw float32s — nothing to do.
    } else if (comp == "zstd") {
#ifdef MICROBOTICA_HAS_ZSTD
        const std::size_t expected =
            static_cast<std::size_t>(schema.total_floats) * 4u;
        decompressed.resize(expected);
        const std::size_t got = ZSTD_decompress(
            decompressed.data(), decompressed.size(), body, body_size);
        if (ZSTD_isError(got)) {
            throw std::runtime_error(
                std::string("zstd decompression failed: ")
                + ZSTD_getErrorName(got));
        }
        decompressed.resize(got);
        floats_ptr = decompressed.data();
        floats_size = decompressed.size();
#else
        throw std::runtime_error(
            "binary frame uses zstd compression, but MICROROBOTICA was built "
            "without libzstd support — rebuild with libzstd available");
#endif
    } else if (comp == "zstd+xor") {
        throw std::runtime_error(
            "binary frame uses 'zstd+xor' compression, which is stateful and "
            "unsupported (the runner does not emit it)");
    } else {
        throw std::runtime_error("unknown binary compression: '" + comp + "'");
    }

    const std::size_t expected_bytes =
        static_cast<std::size_t>(schema.total_floats) * 4u;
    if (floats_size != expected_bytes) {
        throw std::runtime_error(
            "binary frame payload size mismatch: got "
            + std::to_string(floats_size) + " bytes, schema expects "
            + std::to_string(expected_bytes) + " ("
            + std::to_string(schema.total_floats) + " float32s)");
    }

    // Read the flat float32 array.
    std::vector<float> values(static_cast<std::size_t>(schema.total_floats));
    for (std::size_t i = 0; i < values.size(); ++i) {
        values[i] = readF32LE(floats_ptr + i * 4);
    }

    // Walk the schema, mapping each field back into the ResultFrame — the
    // inverse of the runner's _result_frame_to_binary_state():
    //   poses/<actor>.pos  (3 floats) -> positions[<actor>]
    //   poses/<actor>.quat (4 floats) -> orientations[<actor>]
    //   scalars/<name>     (1 float)  -> scalars[<name>]
    for (const auto& field : schema.fields) {
        if (field.offset < 0 || field.count < 0
            || static_cast<std::size_t>(field.offset)
                   + static_cast<std::size_t>(field.count) > values.size()) {
            throw std::runtime_error(
                "binary schema field '" + field.node + "/" + field.field
                + "' is out of range for total_floats="
                + std::to_string(schema.total_floats));
        }
        const float* v = values.data() + field.offset;

        if (field.node == "poses" && field.count == 3
            && endsWith(field.field, ".pos")) {
            const std::string actor =
                field.field.substr(0, field.field.size() - 4);
            frame.positions[actor] = core::Vec3f{v[0], v[1], v[2]};
        } else if (field.node == "poses" && field.count == 4
                   && endsWith(field.field, ".quat")) {
            const std::string actor =
                field.field.substr(0, field.field.size() - 5);
            // Binary quat order is [w, x, y, z], matching core::Quatf.
            frame.orientations[actor] = core::Quatf{v[0], v[1], v[2], v[3]};
        } else if (field.node == "scalars" && field.count == 1) {
            frame.scalars[field.field] = v[0];
        }
        // Any other node/field is ignored. The JSON path also carries
        // meshes, but binary mode never streams them — downstream
        // rendering keeps the last mesh state, exactly as for JSON
        // frames that omit the "meshes" key.
    }

    return frame;
}

bool looksLikeJsonMessage(const std::uint8_t* data, std::size_t size) {
    return size > 0 && data[0] == 0x7B;  // '{'
}

} // namespace microbotica::mime
