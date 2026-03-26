#pragma once

#include <QObject>
#include <QPoint>

#include "core/stability.h"

namespace microbotica::viewport {

/// Orbit/pan/zoom camera controller for the viewport.
///
/// Manages camera parameters using internal doubles (no GfCamera dependency)
/// so the controller works regardless of whether OpenUSD is available.
///
/// Controls:
///   - Left drag:   orbit (azimuth + elevation)
///   - Middle drag:  pan (X/Y offset)
///   - Scroll wheel: zoom (orbit distance)
///
/// Install as an event filter on the target viewport widget.
class CameraController : public QObject {
    Q_OBJECT

public:
    explicit CameraController(QObject* parent = nullptr);
    ~CameraController() override;

    /// Event filter for mouse/wheel events from the viewport widget.
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

    // --- Sensitivity tuning ---

    void setOrbitSensitivity(double s) { orbitSensitivity_ = s; }
    void setPanSensitivity(double s) { panSensitivity_ = s; }
    void setZoomSensitivity(double s) { zoomSensitivity_ = s; }

signals:
    /// Emitted whenever a camera parameter changes.
    void cameraChanged();

private:
    // Camera parameters.
    // Default orbit distance is 0.02 m (20 mm) — suitable for viewing
    // millimeter-scale microrobotic scenes like umr_confinement.usda.
    double orbitDistance_ = 0.02;
    double azimuth_ = 0.0;       // radians
    double elevation_ = 0.3;     // radians (~17 degrees)
    double panX_ = 0.0;
    double panY_ = 0.0;

    // Sensitivity multipliers.
    double orbitSensitivity_ = 0.005;
    double panSensitivity_ = 0.001;
    double zoomSensitivity_ = 0.1;

    // Elevation clamp.
    static constexpr double kMinElevation = -1.5;  // ~-86 degrees
    static constexpr double kMaxElevation =  1.5;  // ~+86 degrees

    // Orbit distance clamp.
    static constexpr double kMinOrbitDistance = 0.0001;  // 0.1 mm
    static constexpr double kMaxOrbitDistance = 100000.0;

    // Mouse tracking state.
    bool orbiting_ = false;
    bool panning_ = false;
    QPoint lastMousePos_;
};

} // namespace microbotica::viewport
