#pragma once

#include <QOpenGLWindow>
#include <QTimer>

#include "core/stability.h"

#ifdef MICROBOTICA_HAS_USD
#include <pxr/base/gf/vec3d.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usdImaging/usdImagingGL/engine.h>
#include <pxr/imaging/glf/simpleLight.h>
#include <pxr/imaging/glf/simpleMaterial.h>
PXR_NAMESPACE_USING_DIRECTIVE
#endif

namespace microbotica::viewport {

class CameraController;

/// Native QOpenGLWindow that renders a USD stage via UsdImagingGLEngine
/// (Hydra/Storm). Embedded into the widget hierarchy by ViewportWidget via
/// QWidget::createWindowContainer — a native GL window presents directly
/// (like glxgears) instead of going through Qt's composited-widget path.
///
/// When USD is not available at build time, displays a placeholder message
/// indicating that OpenUSD rendering is unavailable.
class LocalViewport : public QOpenGLWindow {
    Q_OBJECT

public:
    explicit LocalViewport(QWindow* parent = nullptr);
    ~LocalViewport() override;

#ifdef MICROBOTICA_HAS_USD
    /// Set the USD stage to render.
    void setStage(UsdStageRefPtr stage);
#endif

    /// Access the camera controller for this viewport.
    CameraController* cameraController() const { return cameraController_; }

    /// Set the USD time code for rendering. Used for animation playback.
    /// Default is UsdTimeCode::Default() (static scenes / live simulation).
    void setTimeCode(double t);

    /// Reset time code to default (non-animated).
    void resetTimeCode();

    /// Enable/disable continuous rendering (self-driving render loop).
    void setContinuousRendering(bool enabled);
    bool isContinuousRendering() const { return continuousRendering_; }

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

Q_SIGNALS:
    void renderFailed();

private:
    CameraController* cameraController_ = nullptr;
    QTimer* renderTimer_ = nullptr;
    bool usdAvailable_ = false;
    bool renderFailed_ = false;
    bool continuousRendering_ = false;
    bool useCustomTimeCode_ = false;
    double customTime_ = 0.0;
    int convergencePumpsLeft_ = 0;

#ifdef MICROBOTICA_HAS_USD
    UsdStageRefPtr stage_;
    std::unique_ptr<UsdImagingGLEngine> engine_;

    // Cached lighting state — only key light position updated per frame.
    bool lightingInitialized_ = false;
    GlfSimpleLightVector lights_;
    GlfSimpleMaterial material_;
    GfVec4f sceneAmbient_;

    // Scene bounds (world space), computed once when the stage is set. Used to
    // fit the near/far clip planes snugly so depth-buffer precision stays high.
    GfVec3d sceneCenter_ = GfVec3d(0.0);
    double sceneRadius_ = 0.0;
#endif
};

} // namespace microbotica::viewport
