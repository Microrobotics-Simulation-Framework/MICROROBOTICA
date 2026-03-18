#include "viewport/viewport_widget.h"
#include "viewport/local_viewport.h"
#include "viewport/software_viewport.h"

#include "core/stability.h"
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
        // Lazily create LocalViewport on first request
        if (!localViewport_) {
            localViewport_ = new LocalViewport(this);
            addWidget(localViewport_);
            spdlog::info("ViewportWidget: LocalViewport created on demand");
        }
        setCurrentWidget(localViewport_);
        break;
    case core::RenderMode::Software:
    case core::RenderMode::CloudStream:
        setCurrentWidget(softwareViewport_);
        break;
    }

    currentMode_ = mode;
    renderModeChanged(mode);
}

} // namespace microbotica::viewport
