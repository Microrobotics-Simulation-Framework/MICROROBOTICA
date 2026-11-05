#pragma once

#include <QSurfaceFormat>

#include <string>

#include "viewport/gl_requirements.h"

namespace microbotica::viewport {

/// The surface format every OpenGL context of the viewport asks for: OpenGL 4.5 in the Compatibility profile, with
/// the deprecated functions, a 24-bit depth and an 8-bit stencil buffer, and `samples` samples (none when 0).
///
/// QSurfaceFormat::DeprecatedFunctions is what makes it Compatibility on Mesa. Without it Qt asks for a
/// forward-compatible context (its documented default from OpenGL 3.0 on), and Mesa answers any forward-compatible
/// request with a Core context, whatever profile was asked for: on the Intel GPU and on llvmpipe alike the viewport
/// then got 4.6 or 4.5 Core, and Storm drew nothing (the end-to-end walk of 2026-11-05). The NVIDIA driver ignores the
/// flag, which is why it went unseen.
inline QSurfaceFormat stormSurfaceFormat(int samples) {
    QSurfaceFormat format;
    format.setVersion(kStormGlMajor, kStormGlMinor);
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    format.setOption(QSurfaceFormat::DeprecatedFunctions);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    if (samples > 0) {
        format.setSamples(samples);
    }
    return format;
}

/// stormContextProblem() of a created context's format (QOpenGLContext::format()): "" when Storm can draw in it.
inline std::string stormContextProblem(const QSurfaceFormat& actual) {
    return stormContextProblem(actual.majorVersion(), actual.minorVersion(),
                               actual.profile() == QSurfaceFormat::CompatibilityProfile);
}

} // namespace microbotica::viewport
