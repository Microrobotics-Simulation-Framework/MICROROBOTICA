#include "viewport/software_viewport.h"

#include "core/stability.h"

namespace microbotica::viewport {

SoftwareViewport::SoftwareViewport(QWidget* parent)
    : QLabel(parent)
{
    MBCA_EXPERIMENTAL_WARN("SoftwareViewport");

    setAlignment(Qt::AlignCenter);
    setStyleSheet(
        "QLabel {"
        "  background-color: #2d2d30;"
        "  color: #cccccc;"
        "  font-size: 14px;"
        "}");

    updatePlaceholderText();
}

SoftwareViewport::~SoftwareViewport() = default;

void SoftwareViewport::setStageLoaded(bool loaded)
{
    stageLoaded_ = loaded;
    updatePlaceholderText();
}

void SoftwareViewport::setImage(const QImage& image)
{
    setPixmap(QPixmap::fromImage(image));
}

void SoftwareViewport::updatePlaceholderText()
{
    if (!stageLoaded_) {
        setText(QStringLiteral("Software Viewport \u2014 No active scene"));
    } else {
        setText(QStringLiteral("Software Viewport \u2014 Scene loaded"));
    }
}

} // namespace microbotica::viewport
