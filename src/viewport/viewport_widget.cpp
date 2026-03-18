#include "viewport/viewport_widget.h"
#include "viewport/local_viewport.h"
#include "viewport/software_viewport.h"

#include "core/stability.h"
#include <spdlog/spdlog.h>
#include <QOpenGLContext>

namespace microbotica::viewport {

ViewportWidget::ViewportWidget(QWidget* parent)
    : QStackedWidget(parent)
{
    MBCA_EXPERIMENTAL_WARN("ViewportWidget");

    softwareViewport_ = new SoftwareViewport(this);
    addWidget(softwareViewport_);

    // Only create the OpenGL viewport if a GL context can be created.
    // This prevents "failed to create drawable" crashes in headless/Docker
    // environments without GPU access.
    QOpenGLContext testCtx;
    if (testCtx.create()) {
        localViewport_ = new LocalViewport(this);
        addWidget(localViewport_);
        spdlog::info("ViewportWidget: OpenGL available, LocalViewport created");
    } else {
        spdlog::warn("ViewportWidget: OpenGL not available, using SoftwareViewport only");
    }

    setCurrentWidget(softwareViewport_);
    currentMode_ = core::RenderMode::Software;
}

ViewportWidget::~ViewportWidget() = default;

#ifdef MICROBOTICA_HAS_USD
void ViewportWidget::setStage(UsdStageRefPtr stage)
{
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
    case core::RenderMode::LocalHydra:
        if (localViewport_) {
            setCurrentWidget(localViewport_);
        } else {
            spdlog::warn("ViewportWidget: LocalHydra requested but OpenGL not available, staying on Software");
            mode = core::RenderMode::Software;
        }
        break;
    case core::RenderMode::Software:
    case core::RenderMode::CloudStream:
        setCurrentWidget(softwareViewport_);
        break;
    }

    currentMode_ = mode;
    emit renderModeChanged(mode);
}

} // namespace microbotica::viewport
