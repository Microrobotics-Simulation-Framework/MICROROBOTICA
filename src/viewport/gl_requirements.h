#pragma once

#include <cstdlib>
#include <string>
#include <string_view>

namespace microbotica::viewport {

/// What Hydra Storm needs from OpenGL, and how the viewport treats the renderer it gets.
///
/// Storm (HgiGL, OpenUSD 24.08) needs OpenGL 4.5 in the Compatibility profile: it uses glPushAttrib and other state
/// only that profile keeps. Mesa gives 4.5 Compatibility on its software rasteriser (llvmpipe) and 4.6 on the Intel
/// GPU (iris), so no NVIDIA GPU or driver is needed (MIME 0.3 discovery §3 and §5c: Storm rendered a recording on
/// llvmpipe at GL 4.5 with no GL errors). Header-only and free of Qt, so the viewport and the screenshot test share it.
inline constexpr int kStormGlMajor = 4;
inline constexpr int kStormGlMinor = 5;

/// True when a context of version major.minor is new enough for Storm.
inline bool meetsStormMinimum(int major, int minor) {
    return major > kStormGlMajor || (major == kStormGlMajor && minor >= kStormGlMinor);
}

/// Why Storm cannot draw in a context of version major.minor and this profile, or "" when it can. A Core context of
/// any version is refused: Storm's glPushAttrib and the like fail in it on every frame ("GL error: invalid enum") and
/// nothing is drawn.
inline std::string stormContextProblem(int major, int minor, bool compatibilityProfile) {
    const std::string got = std::to_string(major) + "." + std::to_string(minor)
        + (compatibilityProfile ? " Compatibility" : " Core");
    if (!compatibilityProfile) {
        return "OpenGL " + got + " profile: Storm draws only in the Compatibility profile";
    }
    if (!meetsStormMinimum(major, minor)) {
        return "OpenGL " + got + " profile: below the " + std::to_string(kStormGlMajor) + "."
            + std::to_string(kStormGlMinor) + " Storm needs";
    }
    return {};
}

/// "4.5" — the minimum, as the viewport's messages print it.
inline std::string stormMinimumText() {
    return std::to_string(kStormGlMajor) + "." + std::to_string(kStormGlMinor);
}

/// True when GL_RENDERER names one of Mesa's software rasterisers (llvmpipe, softpipe, swrast).
inline bool isSoftwareRasteriser(std::string_view renderer) {
    for (std::string_view name : {"llvmpipe", "softpipe", "swrast", "Software Rasterizer"}) {
        if (renderer.find(name) != std::string_view::npos) {
            return true;
        }
    }
    return false;
}

/// Multisample count for the viewport's window. Four by default: Storm's alpha-to-coverage, which draws the
/// translucent tube and vessel see-through, needs a multisample buffer, so without one the tube hides the body.
/// MICROROBOTICA_MSAA (0, 2, 4 or 8) overrides it, for a software rasteriser on a large window.
inline int viewportSamples() {
    if (const char* text = std::getenv("MICROROBOTICA_MSAA")) {
        const int samples = std::atoi(text);
        if (samples == 0 || samples == 2 || samples == 4 || samples == 8) {
            return samples;
        }
    }
    return 4;
}

} // namespace microbotica::viewport
