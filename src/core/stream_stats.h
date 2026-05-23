#pragma once

#include <cstdint>
#include <string>

namespace microbotica::core {

/// Live statistics for a ResultFrame stream — for testing and benchmarking
/// the JSON vs binary wire formats (MIME v0.2 fit-up §8).
///
/// Collected at the ZMQ SUB receive point, so the figures reflect the raw
/// wire and decode cost independently of how fast the UI drains and renders
/// frames. A backend that does not stream (stubs) reports the default zeros.
struct StreamStats {
    std::string format = "json";        ///< "json" | "binary"
    std::string compression = "none";   ///< binary only: "none" | "zstd" | ...
    std::uint64_t framesReceived = 0;   ///< ResultFrames decoded off the wire
    std::uint64_t bytesReceived = 0;    ///< total ZMQ payload bytes, frames only
    std::uint64_t paramsMessages = 0;   ///< one-time {"type":"params"} notices
    std::uint64_t decodeErrors = 0;     ///< messages dropped on decode failure
    std::uint32_t lastFrameBytes = 0;   ///< on-wire size of the most recent frame
    double decodeAvgUs = 0.0;           ///< mean decode time per frame (us)
    double decodeMaxUs = 0.0;           ///< worst-case decode time (us)
    double receivedFps = 0.0;           ///< EWMA of the on-wire frame rate
};

} // namespace microbotica::core
