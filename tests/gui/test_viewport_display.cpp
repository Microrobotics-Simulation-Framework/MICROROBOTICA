// MBCA-VER-020: the viewport's OpenGL context and its first frames, on an X display.
//
// What the launcher's offscreen smoke test could not see: Qt's offscreen platform has no OpenGL, and the screenshot
// tests (MBCA-VER-011, -012) make their own EGL context, so neither went through the context Qt makes for the
// viewport's window. On Mesa that context was Core (no QSurfaceFormat::DeprecatedFunctions: Qt asked for a forward-
// compatible one, and Mesa answers that with Core), Storm failed every frame with "GL error: invalid enum" and the
// viewport stayed empty (the end-to-end walk of 2026-11-05). These tests go through Qt's xcb platform and GLX, as the
// app does on a desktop:
//
// - the format the viewport asks for (stormSurfaceFormat()) gives a 4.5+ Compatibility context;
// - the viewport's own window (LocalViewport) draws a cube with no GL error reported and something on screen;
// - with MICROROBOTICA_A0_RUN naming a MIME 0.3 run folder, the same for A0's recording (skipped otherwise).
//
// Skipped without an X display. CI runs them under Xvfb (llvmpipe); scripts/check-display.sh runs them on this
// machine's display, through the Intel GPU by default.

#include <catch2/catch_test_macros.hpp>

#include "gui/gui_app.h"
#include "viewport/local_viewport.h"
#include "viewport/storm_surface_format.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QImage>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>

#include <pxr/base/tf/diagnosticMgr.h>
#include <pxr/base/tf/error.h>
#include <pxr/base/tf/status.h>
#include <pxr/base/tf/warning.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/cube.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xform.h>

#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>

namespace {

using namespace microbotica;

void needXDisplay() {
    if (!testing::onXDisplay()) {
        SKIP("no X display (DISPLAY unset): CI runs these under xvfb-run; scripts/check-display.sh on a desktop");
    }
}

/// Counts the "GL error" diagnostics Storm (HgiGL) reports while it lives: what the walk's terminal printed.
class GlErrorCount : public TfDiagnosticMgr::Delegate {
public:
    GlErrorCount() { TfDiagnosticMgr::GetInstance().AddDelegate(this); }
    ~GlErrorCount() override { TfDiagnosticMgr::GetInstance().RemoveDelegate(this); }
    GlErrorCount(const GlErrorCount&) = delete;
    GlErrorCount& operator=(const GlErrorCount&) = delete;

    void IssueError(const TfError& error) override { note(error.GetCommentary()); }
    void IssueFatalError(const TfCallContext& /*context*/, const std::string& message) override { note(message); }
    void IssueStatus(const TfStatus& /*status*/) override {}
    void IssueWarning(const TfWarning& warning) override { note(warning.GetCommentary()); }

    int errors = 0;
    std::string first;

private:
    void note(const std::string& text) {
        if (text.find("GL error") != std::string::npos && errors++ == 0) {
            first = text;
        }
    }
};

/// The viewport's window, keeping each frame's picture before its buffers swap: the only moment a NoPartialUpdate
/// window's back buffer holds it.
class FrameProbe : public viewport::LocalViewport {
public:
    using LocalViewport::LocalViewport;
    QImage last;
    int frames = 0;
    QString contextVersion;

protected:
    void initializeGL() override {
        LocalViewport::initializeGL();
        const auto* version = reinterpret_cast<const char*>(context()->functions()->glGetString(GL_VERSION));
        contextVersion = QString::fromLatin1(version ? version : "");
    }
    void paintOverGL() override {
        context()->functions()->glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
        last = grabFramebuffer();
        ++frames;
    }
};

/// The share of pixels that differ from the picture's corner (the background) by more than a little.
double contentShare(const QImage& picture) {
    if (picture.isNull()) return 0.0;
    const QImage image = picture.convertToFormat(QImage::Format_RGB32);
    const QRgb background = image.pixel(0, 0);
    long differ = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QRgb p = image.pixel(x, y);
            if (std::abs(qRed(p) - qRed(background)) + std::abs(qGreen(p) - qGreen(background))
                    + std::abs(qBlue(p) - qBlue(background)) > 24) {
                ++differ;
            }
        }
    }
    return static_cast<double>(differ) / (static_cast<double>(image.width()) * image.height());
}

struct Drawn {
    int frames = 0;
    bool failed = false;
    int glErrors = 0;
    std::string firstGlError;
    double content = 0.0;
    QString contextVersion;
};

/// Show the viewport's window on the display with `stage` (at `time`, or the default time code when NaN), let it draw
/// for up to `seconds` or 12 frames, and say what it drew.
Drawn drawInViewport(const UsdStageRefPtr& stage, double time, double seconds = 8.0) {
    GlErrorCount count;
    Drawn drawn;
    {
        FrameProbe window(viewport::viewportSamples());
        QObject::connect(&window, &viewport::LocalViewport::renderFailed, [&drawn]() { drawn.failed = true; });
        window.resize(320, 240);
        window.setStage(stage);
        if (!std::isnan(time)) {
            window.setTimeCode(time);
        }
        window.show();
        QElapsedTimer clock;
        clock.start();
        while (clock.elapsed() < seconds * 1000.0 && window.frames < 12 && !drawn.failed) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        }
        drawn.frames = window.frames;
        drawn.content = contentShare(window.last);
        drawn.contextVersion = window.contextVersion;
        window.setStage(nullptr);
        window.hide();
        QCoreApplication::processEvents();
    }
    drawn.glErrors = count.errors;
    drawn.firstGlError = count.first;
    return drawn;
}

void report(const Drawn& drawn) {
    UNSCOPED_INFO("context: " << drawn.contextVersion.toStdString() << "; frames " << drawn.frames << ", failed "
                  << drawn.failed << ", GL errors " << drawn.glErrors << " (first: " << drawn.firstGlError << ")"
                  << ", content " << drawn.content);
}

} // namespace

TEST_CASE("MBCA-VER-020: the viewport's format gives a Compatibility context on the display", "[gui][display]") {
    needXDisplay();
    QOpenGLContext context;
    context.setFormat(viewport::stormSurfaceFormat(viewport::viewportSamples()));
    REQUIRE(context.create());
    QOffscreenSurface surface;
    surface.setFormat(context.format());
    surface.create();
    REQUIRE(context.makeCurrent(&surface));
    const auto* version = reinterpret_cast<const char*>(context.functions()->glGetString(GL_VERSION));
    const auto* renderer = reinterpret_cast<const char*>(context.functions()->glGetString(GL_RENDERER));
    const std::string text = version ? version : "";
    INFO("GL_VERSION " << text << ", GL_RENDERER " << (renderer ? renderer : ""));
    CHECK(viewport::stormContextProblem(context.format()).empty());
    CHECK(context.format().profile() == QSurfaceFormat::CompatibilityProfile);
    CHECK(text.find("Compatibility") != std::string::npos);
    context.doneCurrent();
}

TEST_CASE("MBCA-VER-020: the viewport draws a cube on the display with no GL error", "[gui][display]") {
    needXDisplay();
    // A 5 mm cube at the origin, in metres with Z up, as the viewport's default orbit (20 mm out) frames it.
    UsdStageRefPtr stage = UsdStage::CreateInMemory();
    UsdGeomSetStageUpAxis(stage, UsdGeomTokens->z);
    UsdGeomSetStageMetersPerUnit(stage, 1.0);
    UsdGeomXform::Define(stage, SdfPath("/World"));
    UsdGeomCube::Define(stage, SdfPath("/World/Cube")).CreateSizeAttr().Set(0.005);

    const Drawn drawn = drawInViewport(stage, std::numeric_limits<double>::quiet_NaN());
    report(drawn);
    CHECK_FALSE(drawn.failed);
    CHECK(drawn.contextVersion.contains(QStringLiteral("Compatibility")));
    CHECK(drawn.glErrors == 0);
    CHECK(drawn.frames >= 3);
    CHECK(drawn.content > 0.01);
}

TEST_CASE("MBCA-VER-020: the viewport draws A0's recording on the display with no GL error", "[gui][display][a0]") {
    const char* folder = std::getenv("MICROROBOTICA_A0_RUN");
    if (!folder || !*folder) {
        SKIP("MICROROBOTICA_A0_RUN names no MIME 0.3 run folder (scripts/make_a0_fixtures.sh writes one)");
    }
    needXDisplay();
    const std::string recording = std::string(folder) + "/recording.usdc";
    REQUIRE(QFileInfo::exists(QString::fromStdString(recording)));
    UsdStageRefPtr stage = UsdStage::Open(recording);
    REQUIRE(stage);
    const double middle = 0.5 * (stage->GetStartTimeCode() + stage->GetEndTimeCode());

    const Drawn drawn = drawInViewport(stage, middle);
    report(drawn);
    CHECK_FALSE(drawn.failed);
    CHECK(drawn.glErrors == 0);
    CHECK(drawn.frames >= 3);
    CHECK(drawn.content > 0.01);
}
