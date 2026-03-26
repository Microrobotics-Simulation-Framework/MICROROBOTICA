#pragma once

#include <QLabel>

#include "core/stability.h"

namespace microbotica::viewport {

/// QLabel-based fallback viewport for when Hydra FBO rendering is intractable.
///
/// Displays a placeholder message when no stage is loaded, and can show
/// simple status text or a rendered QImage when a software renderer
/// is eventually connected.
class SoftwareViewport : public QLabel {
    Q_OBJECT

public:
    explicit SoftwareViewport(QWidget* parent = nullptr);
    ~SoftwareViewport() override;

    /// Notify the viewport whether a stage is currently loaded.
    void setStageLoaded(bool loaded);

    /// Display a rendered image (for future software renderer integration).
    void setImage(const QImage& image);

    /// Check whether a stage is considered loaded.
    bool isStageLoaded() const { return stageLoaded_; }

    /// Display a custom status message (e.g., GL context unavailable).
    void setStatusMessage(const QString& message);

private:
    void updatePlaceholderText();

    bool stageLoaded_ = false;
    QString statusMessage_;
};

} // namespace microbotica::viewport
