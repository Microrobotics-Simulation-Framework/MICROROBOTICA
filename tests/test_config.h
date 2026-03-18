#pragma once

#include <string>

namespace microbotica::test {

/// Returns the absolute path to a test fixture file.
/// Uses MICROBOTICA_SOURCE_DIR defined by CMake.
inline std::string fixturePath(const std::string& relativePath) {
#ifdef MICROBOTICA_SOURCE_DIR
    return std::string(MICROBOTICA_SOURCE_DIR) + "/tests/fixtures/" + relativePath;
#else
    return "tests/fixtures/" + relativePath;
#endif
}

} // namespace microbotica::test
