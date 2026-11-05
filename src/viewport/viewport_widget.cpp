#include "viewport/viewport_widget.h"
#include "viewport/local_viewport.h"
#include "viewport/software_viewport.h"
#include "viewport/gl_requirements.h"
#include "viewport/storm_surface_format.h"

#include "core/stability.h"
#include <QOpenGLContext>
#include <QWidget>
#include <spdlog/spdlog.h>

namespace microbotica::viewport {

ViewportWidget::ViewportWidget(QWidget* parent)
    : QStackedWidget(parent)
{
    MBCA_EXPERIMENTAL_WARN("ViewportWidget");

    softwareViewport_ = new SoftwareViewport(this);
    addWidget(softwareViewport_);

    // Start with SoftwareViewport only. LocalViewport (QOpenGLWidget) is
    // created lazily on first setRenderMode(LocalHydra) call. This avoids
    // "failed to create drawable" in Docker/headless environments where
    // QOpenGLWidget construction triggers an unusable GLX drawable.
    setCurrentWidget(softwareViewport_);
    currentMode_ = core::RenderMode::Software;
}

ViewportWidget::~ViewportWidget() = default;

#ifdef MICROBOTICA_HAS_USD
void ViewportWidget::setStage(UsdStageRefPtr stage)
{
    pendingStage_ = stage;
    if (localViewport_) {
        localViewport_->setStage(stage);
    }
    softwareViewport_->setStageLoaded(stage != nullptr);
}
#else
void ViewportWidget::setStage(void* /*stage*/)
{
    // USD not available; software viewport remains in placeholder state.
}
#endif

void ViewportWidget::setRenderMode(core::RenderMode mode)
{
    if (mode == currentMode_) {
        return;
    }

    switch (mode) {
    case core::RenderMode::LocalHydra: {
        // Lazily create LocalViewport on first request
        if (!localViewport_) {
            // Probe for a usable GL 4.5 Compatibility context (what Storm needs)
            // before creating the GL window. If the driver can't provide it, stay
            // on Software. This prevents "failed to create drawable" crashes on
            // headless setups. Mesa provides it on the Intel GPU (/dev/dri) and on
            // its software rasteriser, llvmpipe; no NVIDIA GPU is needed.
            // The probe asks for exactly what the viewport's window asks for
            // (stormSurfaceFormat()), and refuses a Core context as well as an
            // old one: Storm draws nothing in either.
            QOpenGLContext testCtx;
            testCtx.setFormat(stormSurfaceFormat(0));
            bool ctxOk = testCtx.create();

            if (ctxOk) {
                if (const std::string problem = stormContextProblem(testCtx.format()); !problem.empty()) {
                    ctxOk = false;
                    spdlog::warn("ViewportWidget: GL probe got {} — need {} Compatibility",
                                 problem, stormMinimumText());
                }
            }

            if (!ctxOk) {
                spdlog::warn("ViewportWidget: No usable GL {} Compatibility context — "
                             "falling back to Software viewport", stormMinimumText());
                softwareViewport_->setStatusMessage(QString::fromStdString(
                    "3D Viewport — Software Mode\n\n"
                    "Hydra/Storm needs OpenGL " + stormMinimumText() + " (Compatibility profile).\n"
                    "Scene is loaded and simulation will run — use the\n"
                    "scene hierarchy panel to inspect prims.\n\n"
                    "Mesa provides it: the Intel GPU (docker run --device /dev/dri),\n"
                    "or its software rasteriser (LIBGL_ALWAYS_SOFTWARE=1)."));
                return;
            }

            localViewport_ = new LocalViewport(viewportSamples());
            // LocalViewport is a native QOpenGLWindow — embed it in the widget
            // hierarchy via a window container so it presents directly to the
            // screen instead of through Qt's composited-widget path.
            localViewportContainer_ = QWidget::createWindowContainer(localViewport_, this);
            localViewportContainer_->setFocusPolicy(Qt::StrongFocus);
            addWidget(localViewportContainer_);
            spdlog::info("ViewportWidget: LocalViewport (native GL window) created on demand");

            // If Hydra fails on the first frame, fall back to Software
            connect(localViewport_, &LocalViewport::renderFailed,
                    this, [this]() {
                spdlog::warn("ViewportWidget: Hydra render failed — switching to Software viewport");
                softwareViewport_->setStatusMessage(
                    "3D Viewport — Software Mode\n\n"
                    "Hydra/Storm could not initialize on this GL context.\n"
                    "Scene is loaded and simulation will run — use the\n"
                    "scene hierarchy panel to inspect prims.\n\n"
                    "Try Mesa's software rasteriser: LIBGL_ALWAYS_SOFTWARE=1.");
                setCurrentWidget(softwareViewport_);
                currentMode_ = core::RenderMode::Software;
            });

#ifdef MICROBOTICA_HAS_USD
            // Forward any stage that was set before LocalViewport existed
            if (pendingStage_) {
                localViewport_->setStage(pendingStage_);
            }
#endif
        }
        setCurrentWidget(localViewportContainer_);
        break;
    }
    case core::RenderMode::Software:
    case core::RenderMode::CloudStream:
        setCurrentWidget(softwareViewport_);
        break;
    }

    currentMode_ = mode;
    Q_EMIT renderModeChanged(mode);
}

void ViewportWidget::setTimeCode(double t) {
    if (localViewport_) localViewport_->setTimeCode(t);
}

void ViewportWidget::resetTimeCode() {
    if (localViewport_) localViewport_->resetTimeCode();
}

void ViewportWidget::setContinuousRendering(bool enabled) {
    if (localViewport_) {
        localViewport_->setContinuousRendering(enabled);
    }
}

} // namespace microbotica::viewport
