#pragma once

#include <QList>
#include <QString>
#include <QWidget>

class QEvent;

namespace microbotica::viewport {

/// Floating notice cards drawn on top of the 3D viewport.
///
/// MBCA-COMP-031
///
/// Designed to surface long-running background state (waiting on the MIME
/// runner, decode errors, JIT progress, ...) without blocking the user
/// from interacting with the viewport. Inputs pass straight through — the
/// overlay is purely visual.
///
/// Implementation note: the viewport's local renderer is a native
/// QOpenGLWindow embedded via QWidget::createWindowContainer, and sibling
/// Qt widgets cannot be composited on top of a native window. The overlay
/// is therefore a separate top-level frameless tool window that tracks the
/// geometry of a target widget (the viewport) via an event filter.
///
/// Notices are keyed by a stable string id so the same notice can be
/// updated (changing body text) without flicker, and removed by the same
/// id when the underlying condition resolves.
///
/// Visibility and opacity are persisted under the `notices/` QSettings
/// group via the Settings dialog.
class NoticeOverlay : public QWidget {
    Q_OBJECT

public:
    enum class Severity { Info, Warning, Error };

    /// Construct a top-level overlay that follows the geometry of
    /// `trackTarget` (typically the ViewportWidget). The target widget
    /// owns nothing; it is only watched.
    explicit NoticeOverlay(QWidget* trackTarget);

    /// Add (or replace) the notice with the given id.
    void addNotice(const QString& id, Severity severity,
                   const QString& title, const QString& body);

    /// Remove the notice with the given id. No-op if not present.
    void removeNotice(const QString& id);

    /// Drop every notice.
    void clearNotices();

    bool overlayEnabled() const { return enabled_; }
    void setOverlayEnabled(bool enabled);

    /// Card background opacity, 0..100. Default 85.
    int opacityPercent() const { return opacity_; }
    void setOpacityPercent(int percent);

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Notice {
        QString id;
        Severity severity;
        QString title;
        QString body;
    };

    void syncToTarget();

    QWidget* trackTarget_ = nullptr;
    QList<Notice> notices_;
    bool enabled_ = true;
    int opacity_ = 85;
};

} // namespace microbotica::viewport
