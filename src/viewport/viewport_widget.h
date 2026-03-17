#pragma once

#include <QStackedWidget>

#include "core/component_meta.h"
#include "core/stability.h"
#include "core/types.h"

#ifdef MICROBOTICA_HAS_USD
#include <pxr/usd/usd/stage.h>
PXR_NAMESPACE_USING_DIRECTIVE
#endif

namespace microbotica::viewport {

class LocalViewport;
class SoftwareViewport;

/// QStackedWidget that switches between LocalViewport and SoftwareViewport
/// based on the active RenderMode.
///
/// MBCA-COMP-030
class ViewportWidget : public QStackedWidget {
    Q_OBJECT

public:
    explicit ViewportWidget(QWidget* parent = nullptr);
    ~ViewportWidget() override;

    static const core::ComponentMeta& interfaceMeta() {
        static const core::ComponentMeta meta{
            .component_id      = "MBCA-COMP-030",
            .component_version = "1.0.0",
            .stability         = core::StabilityLevel::Experimental,
            .description       = "Viewport container that switches between local Hydra "
                                 "and software fallback rendering.",
            .preconditions     = {"Qt OpenGL context is available for LocalHydra mode"},
            .postconditions    = {"After setRenderMode(), the visible child widget matches the mode"},
            .invariants        = {"Exactly one child viewport is visible at any time"},
            .design_rationale  = "QStackedWidget provides zero-cost switching between render "
                                 "backends without destroying widget state.",
            .assumptions       = {"Only LocalHydra and Software modes are handled locally; "
                                  "CloudStream is handled by a separate streaming widget"},
            .limitations       = {"CloudStream mode not yet implemented — falls back to Software"},
            .validated_regimes = {},
            .hazard_hints      = {
                "Switching modes during active rendering may cause a brief blank frame.",
            },
            .references        = {},
            .deprecation_notice = std::nullopt,
        };
        return meta;
    }

    /// Set the USD stage for rendering (stub when USD unavailable).
#ifdef MICROBOTICA_HAS_USD
    void setStage(UsdStageRefPtr stage);
#else
    void setStage(void* stage);
#endif

    /// Switch the active render mode.
    void setRenderMode(core::RenderMode mode);

    /// Get the current render mode.
    core::RenderMode renderMode() const { return currentMode_; }

    /// Access the local (Hydra/OpenGL) viewport.
    LocalViewport* localViewport() const { return localViewport_; }

    /// Access the software fallback viewport.
    SoftwareViewport* softwareViewport() const { return softwareViewport_; }

signals:
    void renderModeChanged(microbotica::core::RenderMode mode);

private:
    LocalViewport* localViewport_ = nullptr;
    SoftwareViewport* softwareViewport_ = nullptr;
    core::RenderMode currentMode_ = core::RenderMode::Software;
};

} // namespace microbotica::viewport
