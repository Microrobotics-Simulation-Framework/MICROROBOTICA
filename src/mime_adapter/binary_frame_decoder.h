#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "core/result_frame.h"

namespace microbotica::mime {

/// One field entry from a binary stream schema (MIME v0.2 fit-up §8).
///
/// Each field occupies `count` consecutive float32s starting at `offset`
/// in the decoded float array.
struct BinaryFieldSchema {
    std::string node;    ///< "poses" or "scalars"
    std::string field;   ///< "<actor>.pos", "<actor>.quat", or scalar name
    int offset = 0;      ///< first float32 index in the decoded array
    int count = 0;       ///< number of consecutive float32s
};

/// Decode schema for the binary ResultFrame wire format.
///
/// Delivered by the runner's `stream_info` REP command. Describes how to
/// map a flat float32 array back into a ResultFrame. The runner builds its
/// encoder lazily, so `stream_info` returns `schema: null` until the first
/// binary frame is published — a consumer must re-query until it appears.
struct BinaryStreamSchema {
    std::vector<BinaryFieldSchema> fields;
    int total_floats = 0;
    int frame_bytes = 0;               ///< uncompressed size (8 + 4*total_floats)
    std::string compression = "none";  ///< "none" | "zstd" | "zstd+xor"

    /// True once a usable (non-empty) schema has been parsed.
    bool valid() const { return total_floats > 0 && !fields.empty(); }
};

/// Parse a binary stream schema from the `schema` object of a `stream_info`
/// reply. Throws std::runtime_error / nlohmann::json::exception if the JSON
/// is malformed.
BinaryStreamSchema parseBinarySchema(const nlohmann::json& schema_json);

/// Decode one binary ResultFrame into the same struct the JSON path produces.
///
/// Wire layout (MIME v0.2 fit-up §8):
///   [ 8 bytes : sim_time, IEEE-754 float64, little-endian ]
///   [ payload ]
/// For `compression == "none"` the payload is `total_floats` float32s,
/// little-endian, in schema field order. For `"zstd"` the payload is
/// zstd-compressed and decompressed first (requires libzstd at build time).
/// `"zstd+xor"` is stateful and unsupported — the runner never emits it.
///
/// Throws std::runtime_error on any structural error (short frame, payload
/// size mismatch, unsupported compression, missing zstd support).
core::ResultFrame decodeBinaryFrame(const std::uint8_t* data,
                                    std::size_t size,
                                    const BinaryStreamSchema& schema);

/// True if a received message looks like a JSON text message (starts with
/// '{' = 0x7B), false if it should be treated as a binary frame.
///
/// A JSON message from the runner always begins with 0x7B; a binary frame
/// begins with the low byte of an IEEE-754 float64 sim_time and only
/// collides with 0x7B ~1/256 of the time. The caller resolves that residual
/// ambiguity by attempting a JSON parse and falling back to binary decoding
/// if it fails.
bool looksLikeJsonMessage(const std::uint8_t* data, std::size_t size);

} // namespace microbotica::mime
