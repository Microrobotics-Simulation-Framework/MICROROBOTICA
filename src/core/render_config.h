#pragma once

#include <string>
#include "types.h"

namespace microbotica::core {

/// Configuration for creating a render session.
struct RenderConfig {
    RenderMode mode = RenderMode::LocalHydra;
    int width = 1280;
    int height = 720;
};

} // namespace microbotica::core
