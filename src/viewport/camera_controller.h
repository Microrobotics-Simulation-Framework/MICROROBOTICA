#pragma once

#include <QObject>
#include <QPoint>

#include "core/stability.h"

namespace microbotica::viewport {

/// Orbit/pan/zoom camera controller for the viewport.
///
/// Controls (Blender/Maya-inspired, laptop-friendly):
///   - Left drag or Alt+Left drag: orbit (azimuth + elevation)
///   - Right drag, Middle drag, or Shift+Left drag: pan
///   - Scroll wheel or +/-: zoom
///   - Arrow keys: orbit in small steps
///   - WASD: pan
///   - 1-6: axis-snap views (Front/Back/Top/Bottom/Left/Right)
///   - R or Home: reset camera
///   - F1 or ?: show controls help
class CameraController : public QObject {
    Q_OBJECT

public:
    explicit CameraController(QObject* parent = nullptr);
    ~CameraController() override;

    bool eventFilter(QObject* watched, QEvent* event) override;

    // --- Camera parameter accessors ---

    double orbitDistance() const { return orbitDistance_; }
    void setOrbitDistance(double d);

    double azimuth() const { return azimuth_; }
    void setAzimuth(double a);

    double elevation() const { return elevation_; }
    void setElevation(double e);

    double panX() const { return panX_; }
    double panY() const { return panY_; }
    void setPan(double x, double y);

    /// Reset camera to default position.
    void resetToDefault();

    /// Frame the scene — set orbit distance to show objects of the given radius.
    void frameRadius(double radius);

    // --- Axis-snap views ---

    void snapFront();
    void snapBack();
    void snapTop();
    void snapBottom();
    void snapLeft();
    void snapRight();

    // --- Sensitivity tuning ---

    void setOrbitSensitivity(double s) { orbitSensitivity_ = s; }
    void setPanSensitivity(double s) { panSensitivity_ = s; }
    void setZoomSensitivity(double s) { zoomSensitivity_ = s; }

Q_SIGNALS:
    void cameraChanged();
    void helpRequested();

private:
    // Camera parameters.
    double orbitDistance_ = 0.02;
    double azimuth_ = 0.0;       // radians
    double elevation_ = 0.3;     // radians (~17 degrees)
    double panX_ = 0.0;
    double panY_ = 0.0;

    // Sensitivity multipliers.
    double orbitSensitivity_ = 0.005;
    double panSensitivity_ = 0.001;
    double zoomSensitivity_ = 0.1;

    // Elevation clamp — nearly full sphere (±89 degrees).
    static constexpr double kMinElevation = -1.553;  // ~-89 degrees
    static constexpr double kMaxElevation =  1.553;  // ~+89 degrees

    static constexpr double kMinOrbitDistance = 0.0001;
    static constexpr double kMaxOrbitDistance = 100000.0;

    // Mouse tracking state.
    bool orbiting_ = false;
    bool panning_ = false;
    QPoint lastMousePos_;
};

} // namespace microbotica::viewport
