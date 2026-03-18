#include "viewport/camera_controller.h"

#include "core/stability.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace microbotica::viewport {

CameraController::CameraController(QObject* parent)
    : QObject(parent)
{
    MBCA_EXPERIMENTAL_WARN("CameraController");
}

CameraController::~CameraController() = default;

bool CameraController::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            orbiting_ = true;
            lastMousePos_ = me->pos();
            return true;
        }
        if (me->button() == Qt::MiddleButton) {
            panning_ = true;
            lastMousePos_ = me->pos();
            return true;
        }
        break;
    }

    case QEvent::MouseButtonRelease: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton && orbiting_) {
            orbiting_ = false;
            return true;
        }
        if (me->button() == Qt::MiddleButton && panning_) {
            panning_ = false;
            return true;
        }
        break;
    }

    case QEvent::MouseMove: {
        auto* me = static_cast<QMouseEvent*>(event);
        const QPoint delta = me->pos() - lastMousePos_;
        lastMousePos_ = me->pos();

        if (orbiting_) {
            azimuth_ += delta.x() * orbitSensitivity_;
            elevation_ += delta.y() * orbitSensitivity_;
            elevation_ = std::clamp(elevation_, kMinElevation, kMaxElevation);
            cameraChanged();
            if (auto* w = qobject_cast<QWidget*>(watched)) {
                w->update();
            }
            return true;
        }

        if (panning_) {
            panX_ += delta.x() * panSensitivity_ * (orbitDistance_ * 0.1);
            panY_ -= delta.y() * panSensitivity_ * (orbitDistance_ * 0.1);
            cameraChanged();
            if (auto* w = qobject_cast<QWidget*>(watched)) {
                w->update();
            }
            return true;
        }
        break;
    }

    case QEvent::Wheel: {
        auto* we = static_cast<QWheelEvent*>(event);
        const double degrees = we->angleDelta().y() / 8.0;
        const double steps = degrees / 15.0;

        orbitDistance_ -= steps * zoomSensitivity_ * orbitDistance_;
        orbitDistance_ = std::clamp(orbitDistance_, kMinOrbitDistance, kMaxOrbitDistance);

        cameraChanged();
        if (auto* w = qobject_cast<QWidget*>(watched)) {
            w->update();
        }
        return true;
    }

    default:
        break;
    }

    return QObject::eventFilter(watched, event);
}

void CameraController::setOrbitDistance(double d)
{
    orbitDistance_ = std::clamp(d, kMinOrbitDistance, kMaxOrbitDistance);
    cameraChanged();
}

void CameraController::setAzimuth(double a)
{
    azimuth_ = a;
    cameraChanged();
}

void CameraController::setElevation(double e)
{
    elevation_ = std::clamp(e, kMinElevation, kMaxElevation);
    cameraChanged();
}

void CameraController::setPan(double x, double y)
{
    panX_ = x;
    panY_ = y;
    cameraChanged();
}

void CameraController::resetToDefault()
{
    orbitDistance_ = 10.0;
    azimuth_ = 0.0;
    elevation_ = 0.3;
    panX_ = 0.0;
    panY_ = 0.0;
    cameraChanged();
}

} // namespace microbotica::viewport
