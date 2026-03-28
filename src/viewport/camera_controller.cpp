#include "viewport/camera_controller.h"

#include "core/stability.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace microbotica::viewport {

static constexpr double kPi = 3.14159265358979323846;

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
        bool shift = me->modifiers() & Qt::ShiftModifier;

        if (me->button() == Qt::LeftButton) {
            // Shift+Left = pan, Alt+Left = orbit, plain Left = orbit
            if (shift) {
                panning_ = true;
            } else {
                orbiting_ = true;
            }
            lastMousePos_ = me->pos();
            return true;
        }
        if (me->button() == Qt::MiddleButton || me->button() == Qt::RightButton) {
            panning_ = true;
            lastMousePos_ = me->pos();
            return true;
        }
        break;
    }

    case QEvent::MouseButtonRelease: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            orbiting_ = false;
            panning_ = false;
            return true;
        }
        if (me->button() == Qt::MiddleButton || me->button() == Qt::RightButton) {
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
            // Negate both: drag-right rotates scene rightward (azimuth decreases),
            // drag-up tilts view upward (elevation increases, but Qt Y is inverted).
            azimuth_ -= delta.x() * orbitSensitivity_;
            elevation_ -= delta.y() * orbitSensitivity_;
            elevation_ = std::clamp(elevation_, kMinElevation, kMaxElevation);
            Q_EMIT cameraChanged();
            if (auto* w = qobject_cast<QWidget*>(watched)) w->update();
            return true;
        }

        if (panning_) {
            panX_ += delta.x() * panSensitivity_ * (orbitDistance_ * 0.1);
            panY_ -= delta.y() * panSensitivity_ * (orbitDistance_ * 0.1);
            Q_EMIT cameraChanged();
            if (auto* w = qobject_cast<QWidget*>(watched)) w->update();
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

        Q_EMIT cameraChanged();
        if (auto* w = qobject_cast<QWidget*>(watched)) w->update();
        return true;
    }

    case QEvent::KeyPress: {
        auto* ke = static_cast<QKeyEvent*>(event);
        const double orbitStep = 0.05;
        const double zoomFactor = 0.15;

        switch (ke->key()) {
        case Qt::Key_Left:  azimuth_ += orbitStep; break;
        case Qt::Key_Right: azimuth_ -= orbitStep; break;
        case Qt::Key_Up:
            elevation_ += orbitStep;
            elevation_ = std::clamp(elevation_, kMinElevation, kMaxElevation);
            break;
        case Qt::Key_Down:
            elevation_ -= orbitStep;
            elevation_ = std::clamp(elevation_, kMinElevation, kMaxElevation);
            break;
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            orbitDistance_ *= (1.0 - zoomFactor);
            orbitDistance_ = std::clamp(orbitDistance_, kMinOrbitDistance, kMaxOrbitDistance);
            break;
        case Qt::Key_Minus:
            orbitDistance_ *= (1.0 + zoomFactor);
            orbitDistance_ = std::clamp(orbitDistance_, kMinOrbitDistance, kMaxOrbitDistance);
            break;
        case Qt::Key_Home:
        case Qt::Key_R:
            resetToDefault();
            break;
        case Qt::Key_W: panY_ += orbitDistance_ * 0.05; break;
        case Qt::Key_S: panY_ -= orbitDistance_ * 0.05; break;
        case Qt::Key_A: panX_ -= orbitDistance_ * 0.05; break;
        case Qt::Key_D: panX_ += orbitDistance_ * 0.05; break;

        // Axis-snap views (numpad and number keys)
        case Qt::Key_1: snapFront(); break;
        case Qt::Key_2: snapBack(); break;
        case Qt::Key_3: snapTop(); break;
        case Qt::Key_4: snapBottom(); break;
        case Qt::Key_5: snapLeft(); break;
        case Qt::Key_6: snapRight(); break;

        case Qt::Key_F1:
        case Qt::Key_Question:
            Q_EMIT helpRequested();
            return true;

        default:
            return QObject::eventFilter(watched, event);
        }

        Q_EMIT cameraChanged();
        if (auto* w = qobject_cast<QWidget*>(watched)) w->update();
        return true;
    }

    default:
        break;
    }

    return QObject::eventFilter(watched, event);
}

void CameraController::setOrbitDistance(double d) {
    orbitDistance_ = std::clamp(d, kMinOrbitDistance, kMaxOrbitDistance);
    Q_EMIT cameraChanged();
}

void CameraController::setAzimuth(double a) {
    azimuth_ = a;
    Q_EMIT cameraChanged();
}

void CameraController::setElevation(double e) {
    elevation_ = std::clamp(e, kMinElevation, kMaxElevation);
    Q_EMIT cameraChanged();
}

void CameraController::setPan(double x, double y) {
    panX_ = x;
    panY_ = y;
    Q_EMIT cameraChanged();
}

void CameraController::resetToDefault() {
    orbitDistance_ = 0.02;
    azimuth_ = 0.0;
    elevation_ = 0.3;
    panX_ = 0.0;
    panY_ = 0.0;
    Q_EMIT cameraChanged();
}

void CameraController::frameRadius(double radius) {
    orbitDistance_ = std::clamp(radius * 3.0, kMinOrbitDistance, kMaxOrbitDistance);
    panX_ = 0.0;
    panY_ = 0.0;
    Q_EMIT cameraChanged();
}

// Axis-snap views: set azimuth/elevation to look along an axis.
// Keep orbit distance and pan as-is.

void CameraController::snapFront() {
    azimuth_ = 0.0;
    elevation_ = 0.0;
    Q_EMIT cameraChanged();
}

void CameraController::snapBack() {
    azimuth_ = kPi;
    elevation_ = 0.0;
    Q_EMIT cameraChanged();
}

void CameraController::snapTop() {
    azimuth_ = 0.0;
    elevation_ = kPi / 2.0 - 0.01;  // Just under 90 to avoid gimbal lock
    Q_EMIT cameraChanged();
}

void CameraController::snapBottom() {
    azimuth_ = 0.0;
    elevation_ = -(kPi / 2.0 - 0.01);
    Q_EMIT cameraChanged();
}

void CameraController::snapLeft() {
    azimuth_ = -kPi / 2.0;
    elevation_ = 0.0;
    Q_EMIT cameraChanged();
}

void CameraController::snapRight() {
    azimuth_ = kPi / 2.0;
    elevation_ = 0.0;
    Q_EMIT cameraChanged();
}

} // namespace microbotica::viewport
