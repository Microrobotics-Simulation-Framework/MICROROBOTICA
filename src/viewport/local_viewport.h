#pragma once

#include <QOpenGLWidget>

#include "core/stability.h"

#ifdef MICROBOTICA_HAS_USD
#include <pxr/usd/usd/stage.h>
#include <pxr/usdImaging/usdImagingGL/engine.h>
PXR_NAMESPACE_USING_DIRECTIVE
#endif

namespace microbotica::viewport {

class CameraController;

/// QOpenGLWidget that renders a USD stage via UsdImagingGLEngine (Hydra/Storm).
///
/// When USD is not available at build time, displays a placeholder message
/// indicating that OpenUSD rendering is unavailable.
class LocalViewport : public QOpenGLWidget {
    Q_OBJECT

public:
    explicit LocalViewport(QWidget* parent = nullptr);
    ~LocalViewport() override;

#ifdef MICROBOTICA_HAS_USD
    /// Set the USD stage to render.
    void setStage(UsdStageRefPtr stage);
#endif

    /// Access the camera controller for this viewport.
    CameraController* cameraController() const { return cameraController_; }

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

Q_SIGNALS:
    /// Emitted after the first Render() if HgiGL produces GL errors,
    /// indicating Storm cannot render on this GL context.
    void renderFailed();

private:
    CameraController* cameraController_ = nullptr;
    bool usdAvailable_ = false;
    bool renderFailed_ = false;

#ifdef MICROBOTICA_HAS_USD
    UsdStageRefPtr stage_;
    std::unique_ptr<UsdImagingGLEngine> engine_;
#endif
};

} // namespace microbotica::viewport
