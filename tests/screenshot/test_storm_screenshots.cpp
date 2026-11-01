// Screenshot tests: Hydra Storm drawing recordings on Mesa's software rasteriser (llvmpipe) at OpenGL 4.5
// Compatibility, headless, through the viewport's own StormRenderer and the scene manager's three-layer stage. No
// NVIDIA GPU or driver anywhere (MIME 0.3 discovery §3, §5c).
//
// - The committed demo recording (tests/fixtures/umr_demo.usdc) is drawn at three time codes and compared with the
//   pictures in tests/fixtures/screenshots/, within a tolerance: this runs in CI.
// - A0's recording, from a MIME 0.3 run folder named by MICROROBOTICA_A0_RUN (scripts/make_a0_fixtures.sh writes one),
//   is drawn whole and close up on the body; its pictures are checked for content and motion and written for viewing,
//   never committed (the run folder stays local: design #198). Skipped when the variable is not set.
//
// Pictures are written to MICROROBOTICA_SCREENSHOT_DIR (default <build>/screenshots). With
// MICROROBOTICA_UPDATE_SCREENSHOTS=1 the demo's pictures replace the committed ones.

#include <catch2/catch_test_macros.hpp>

#include "screenshot/headless_gl.h"
#include "core/verification_registry.h"
#include "scene/scene_manager.h"
#include "viewport/gl_requirements.h"
#include "viewport/storm_renderer.h"

#include <QDir>
#include <QFileInfo>
#include <QImage>

#include <pxr/base/gf/range3d.h>
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/tokens.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using microbotica::testing::HeadlessGl;
using microbotica::viewport::OrbitView;
using microbotica::viewport::StormRenderer;

namespace {

constexpr int kWidth = 640;
constexpr int kHeight = 480;
constexpr double kPi = 3.14159265358979323846;

std::string env(const char* name) {
    const char* value = std::getenv(name);
    return value ? std::string(value) : std::string();
}

QString outputDir() {
    const std::string dir = env("MICROROBOTICA_SCREENSHOT_DIR");
    const QString path = dir.empty() ? QStringLiteral(MICROBOTICA_BINARY_DIR "/screenshots") : QString::fromStdString(dir);
    QDir().mkpath(path);
    return path;
}

QImage toImage(const std::vector<std::uint8_t>& rgba) {
    return QImage(rgba.data(), kWidth, kHeight, QImage::Format_RGBA8888).copy();
}

/// Share of pixels that are not the viewport's clear colour.
double drawnShare(const std::vector<std::uint8_t>& rgba) {
    std::size_t drawn = 0;
    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
        const int d = std::abs(rgba[i] - 46) + std::abs(rgba[i + 1] - 46) + std::abs(rgba[i + 2] - 51);
        if (d > 12) {
            ++drawn;
        }
    }
    return static_cast<double>(drawn) / (static_cast<double>(rgba.size()) / 4.0);
}

/// Share of pixels where two pictures differ by more than `tolerance` in some channel.
double differingShare(const QImage& a, const QImage& b, int tolerance) {
    if (a.size() != b.size()) {
        return 1.0;
    }
    const QImage x = a.convertToFormat(QImage::Format_RGBA8888);
    const QImage y = b.convertToFormat(QImage::Format_RGBA8888);
    std::size_t differ = 0;
    for (int row = 0; row < x.height(); ++row) {
        const uchar* p = x.constScanLine(row);
        const uchar* q = y.constScanLine(row);
        for (int col = 0; col < x.width(); ++col) {
            for (int c = 0; c < 3; ++c) {
                if (std::abs(p[4 * col + c] - q[4 * col + c]) > tolerance) {
                    ++differ;
                    break;
                }
            }
        }
    }
    return static_cast<double>(differ) / (static_cast<double>(x.width()) * x.height());
}

/// A view of `range` from the front right and above, the whole box in frame.
OrbitView framing(const GfRange3d& range, double azimuth = 0.9, double elevation = 0.45) {
    OrbitView view;
    const GfVec3d centre = range.GetMidpoint();
    const double radius = std::max(1e-6, 0.5 * range.GetSize().GetLength());
    view.panX = centre[0];
    view.panY = centre[1];
    view.panZ = centre[2];
    view.azimuth = azimuth;
    view.elevation = elevation;
    view.distance = 1.1 * radius / std::sin(0.5 * view.fovDegrees * kPi / 180.0);
    return view;
}

GfRange3d worldBounds(const UsdStageRefPtr& stage, const SdfPath& path, double time) {
    UsdGeomBBoxCache cache(UsdTimeCode(time), {UsdGeomTokens->default_, UsdGeomTokens->render, UsdGeomTokens->proxy},
                           /*useExtentsHint*/ true);
    return cache.ComputeWorldBound(stage->GetPrimAtPath(path)).ComputeAlignedRange();
}

struct Shot {
    std::vector<std::uint8_t> pixels;
    int passes = 0;
    double firstSeconds = 0.0;
    double warmSeconds = 0.0;
    GLenum glError = GL_NO_ERROR;
};

/// One picture: render until Storm converges (bounded), then time a repaint.
Shot shoot(HeadlessGl& gl, StormRenderer& storm, const OrbitView& view, double time) {
    using clock = std::chrono::steady_clock;
    Shot shot;
    HeadlessGl::drainErrors();
    auto t0 = clock::now();
    bool converged = false;
    while (!converged && shot.passes < 50) {
        const std::uint32_t fbo = gl.beginFrame();
        converged = storm.render(view, UsdTimeCode(time), gl.width(), gl.height(), fbo);
        ++shot.passes;
    }
    shot.pixels = gl.endFrame();
    shot.firstSeconds = std::chrono::duration<double>(clock::now() - t0).count();
    // A repaint's cost: the median of three, after the picture (the first can still be compiling a material).
    std::vector<double> warm;
    for (int i = 0; i < 3; ++i) {
        t0 = clock::now();
        storm.render(view, UsdTimeCode(time), gl.width(), gl.height(), gl.beginFrame());
        glFinish();
        warm.push_back(std::chrono::duration<double>(clock::now() - t0).count());
    }
    std::sort(warm.begin(), warm.end());
    shot.warmSeconds = warm[1];
    shot.glError = HeadlessGl::drainErrors();
    return shot;
}

void report(const char* name, const Shot& shot) {
    std::printf("  %-24s passes=%d first=%.3f s warm=%.4f s drawn=%.4f gl_error=0x%x\n", name, shot.passes,
                shot.firstSeconds, shot.warmSeconds, drawnShare(shot.pixels), shot.glError);
}

bool wantLlvmpipe() {
    return env("MICROROBOTICA_SCREENSHOT_GPU") != "1";
}

} // namespace

REGISTER_VERIFICATION_BENCHMARK(
    MBCA_VER_011,
    "MBCA-VER-011",
    "MBCA-COMP-032",
    microbotica::core::BenchmarkType::Regression,
    "Storm draws the demo recording on llvmpipe at GL 4.5 Compatibility as the committed pictures",
    __FILE__,
    "Storm draws the demo recording on Mesa at GL 4.5 Compatibility, as the committed pictures"
)

REGISTER_VERIFICATION_BENCHMARK(
    MBCA_VER_012,
    "MBCA-VER-012",
    "MBCA-COMP-032",
    microbotica::core::BenchmarkType::DataIntegrity,
    "A MIME 0.3 recording (A0) composes over its run folder's stage and draws, moving, on llvmpipe",
    __FILE__,
    "Storm draws A0's MIME 0.3 recording on llvmpipe: the arm's bones, the tube and the body, moving"
)

TEST_CASE("Storm draws the demo recording on Mesa at GL 4.5 Compatibility, as the committed pictures",
          "[screenshot]") {
    HeadlessGl gl(kWidth, kHeight);
    if (!gl.ok()) {
        SKIP("no headless OpenGL here: " + gl.why());
    }
    std::printf("GL: %s | %s | %s\n", gl.vendor().c_str(), gl.renderer().c_str(), gl.version().c_str());
    REQUIRE(microbotica::viewport::meetsStormMinimum(gl.glMajor(), gl.glMinor()));
    if (wantLlvmpipe()) {
        REQUIRE(microbotica::viewport::isSoftwareRasteriser(gl.renderer()));
    }

    microbotica::scene::SceneManager scene;
    REQUIRE(scene.loadScene(MICROBOTICA_SOURCE_DIR "/tests/fixtures/umr_demo.usdc"));
    REQUIRE(scene.hasAnimation());
    StormRenderer storm;
    storm.setStage(scene.stage());
    const OrbitView view = framing(worldBounds(scene.stage(), SdfPath("/World/Environment/Vessel"),
                                               scene.startTimeCode()), 0.9, 0.35);

    const QString goldenDir = QStringLiteral(MICROBOTICA_SOURCE_DIR "/tests/fixtures/screenshots");
    const bool update = env("MICROROBOTICA_UPDATE_SCREENSHOTS") == "1";
    const double start = scene.startTimeCode();
    const double end = scene.endTimeCode();
    std::vector<QImage> pictures;
    for (double t : {start, std::round(0.5 * (start + end)), end}) {
        const std::string name = "umr_demo_t" + std::to_string(static_cast<int>(t));
        const Shot shot = shoot(gl, storm, view, t);
        report(name.c_str(), shot);
        CHECK(shot.glError == GL_NO_ERROR);
        CHECK(drawnShare(shot.pixels) > 0.02);
        CHECK(shot.warmSeconds < 0.5);
        const QImage picture = toImage(shot.pixels);
        picture.save(outputDir() + "/" + QString::fromStdString(name) + ".png");
        pictures.push_back(picture);

        const QString golden = goldenDir + "/" + QString::fromStdString(name) + ".png";
        if (update) {
            QDir().mkpath(goldenDir);
            REQUIRE(picture.save(golden));
            continue;
        }
        if (wantLlvmpipe()) {  // the committed pictures are llvmpipe's
            QImage expected(golden);
            REQUIRE_FALSE(expected.isNull());
            const double share = differingShare(picture, expected, 24);
            std::printf("  %-24s differs from the committed picture at %.4f of pixels\n", name.c_str(), share);
            CHECK(share < 0.005);
        }
    }
    CHECK(storm.rendererId().find("Storm") != std::string::npos);
    // The recording moves: its first and last pictures differ.
    CHECK(differingShare(pictures.front(), pictures.back(), 24) > 0.0005);
}

TEST_CASE("Storm draws A0's MIME 0.3 recording on llvmpipe: the arm's bones, the tube and the body, moving",
          "[screenshot][a0]") {
    const std::string run = env("MICROROBOTICA_A0_RUN");
    if (run.empty()) {
        SKIP("MICROROBOTICA_A0_RUN names no A0 run folder (scripts/make_a0_fixtures.sh writes one)");
    }
    const std::string recording = run + "/recording.usdc";
    REQUIRE(QFileInfo::exists(QString::fromStdString(recording)));
    HeadlessGl gl(kWidth, kHeight);
    if (!gl.ok()) {
        SKIP("no headless OpenGL here: " + gl.why());
    }
    if (wantLlvmpipe()) {
        REQUIRE(microbotica::viewport::isSoftwareRasteriser(gl.renderer()));
    }

    microbotica::scene::SceneManager scene;
    REQUIRE(scene.loadScene(recording));
    REQUIRE(scene.hasAnimation());
    const UsdStageRefPtr stage = scene.stage();
    // The recording composes its run folder's own stage (a relative sublayer): the body, the arm's frames and the
    // field's arrow are there to draw.
    REQUIRE(stage->GetPrimAtPath(SdfPath("/World/umr")).IsValid());
    REQUIRE(stage->GetPrimAtPath(SdfPath("/World/rpm/arm/link_1")).IsValid());
    REQUIRE(stage->GetPrimAtPath(SdfPath("/World/rpm/magnetic/magnetic_torque_umr/arrow")).IsValid());

    StormRenderer storm;
    storm.setStage(stage);
    const double start = scene.startTimeCode();
    const double end = scene.endTimeCode();
    const double middle = std::round(0.5 * (start + end));

    GfRange3d whole;
    for (double t : {start, end}) {
        whole.UnionWith(worldBounds(stage, SdfPath("/World"), t));
    }
    const OrbitView wide = framing(whole, 0.9, 0.35);
    std::vector<QImage> closeUps;
    for (double t : {start, middle, end}) {
        const std::string name = "a0_whole_t" + std::to_string(static_cast<int>(t));
        const Shot shot = shoot(gl, storm, wide, t);
        report(name.c_str(), shot);
        CHECK(shot.glError == GL_NO_ERROR);
        CHECK(drawnShare(shot.pixels) > 0.01);
        CHECK(shot.warmSeconds < 0.5);
        toImage(shot.pixels).save(outputDir() + "/" + QString::fromStdString(name) + ".png");

        // Close up on the body, a box five times its size about it: the body, the tube about it, the field's arrow.
        GfRange3d body = worldBounds(stage, SdfPath("/World/umr"), t);
        const GfVec3d c = body.GetMidpoint();
        const GfVec3d half = 2.5 * body.GetSize();
        const OrbitView close = framing(GfRange3d(c - half, c + half), 0.9, 0.5);
        const std::string closeName = "a0_body_t" + std::to_string(static_cast<int>(t));
        const Shot near = shoot(gl, storm, close, t);
        report(closeName.c_str(), near);
        CHECK(near.glError == GL_NO_ERROR);
        CHECK(drawnShare(near.pixels) > 0.05);
        const QImage picture = toImage(near.pixels);
        picture.save(outputDir() + "/" + QString::fromStdString(closeName) + ".png");
        closeUps.push_back(picture);
    }
    // The body turns and the field's arrow turns with the rotor: the close-ups differ from one another.
    CHECK(differingShare(closeUps[0], closeUps[1], 24) > 0.001);
    CHECK(differingShare(closeUps[1], closeUps[2], 24) > 0.001);
}
