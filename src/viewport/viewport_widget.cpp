#include "viewport/viewport_widget.h"
#include "viewport/local_viewport.h"
#include "viewport/software_viewport.h"

#include "core/stability.h"

namespace microbotica::viewport {

ViewportWidget::ViewportWidget(QWidget* parent)
    : QStackedWidget(parent)
{
    MBCA_EXPERIMENTAL_WARN("ViewportWidget");

    localViewport_ = new LocalViewport(this);
    softwareViewport_ = new SoftwareViewport(this);

    addWidget(localViewport_);
    addWidget(softwareViewport_);

    // Default to software fallback until explicitly switched.
    setCurrentWidget(softwareViewport_);
    currentMode_ = core::RenderMode::Software;
}

ViewportWidget::~ViewportWidget() = default;

#ifdef MICROBOTICA_HAS_USD
void ViewportWidget::setStage(UsdStageRefPtr stage)
{
    localViewport_->setStage(stage);
    softwareViewport_->setStageLoaded(stage != nullptr);
}
#else
void ViewportWidget::setStage(void* /*stage*/)
{
    // USD not available; nothing to do for local viewport.
    // Software viewport remains in placeholder state.
}
#endif

void ViewportWidget::setRenderMode(core::RenderMode mode)
{
    if (mode == currentMode_) {
        return;
    }

    switch (mode) {
    case core::RenderMode::LocalHydra:
        setCurrentWidget(localViewport_);
        break;
    case core::RenderMode::Software:
    case core::RenderMode::CloudStream:
        // CloudStream not yet implemented — fall back to Software.
        setCurrentWidget(softwareViewport_);
        break;
    }

    currentMode_ = mode;
    emit renderModeChanged(mode);
}

} // namespace microbotica::viewport
