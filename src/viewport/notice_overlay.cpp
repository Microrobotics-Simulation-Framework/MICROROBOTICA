#include "viewport/notice_overlay.h"

#include <QColor>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPoint>
#include <QRect>
#include <QSize>

namespace microbotica::viewport {

namespace {

// Severity → accent colour and glyph. Palette inspired by noice.nvim's
// floating message windows: an icon + the title rendered in the severity
// hue, the body dimmer, on a near-opaque rounded panel with a thin
// severity-coloured border.
struct SeverityStyle {
    QColor accent;
    QString glyph;
};

SeverityStyle styleFor(NoticeOverlay::Severity sev) {
    switch (sev) {
        case NoticeOverlay::Severity::Warning:
            return {QColor(0xE5, 0xC0, 0x7B), QStringLiteral("▲")}; // ▲
        case NoticeOverlay::Severity::Error:
            return {QColor(0xE5, 0x7A, 0x7A), QStringLiteral("✖")}; // ✖
        case NoticeOverlay::Severity::Info:
        default:
            return {QColor(0x8E, 0xC0, 0x7C), QStringLiteral("●")}; // ●
    }
}

constexpr int kCardWidth     = 360;
constexpr int kCardPaddingX  = 14;
constexpr int kCardPaddingY  = 10;
constexpr int kCardSpacing   = 8;
constexpr int kGlyphGap      = 10;
constexpr int kTopMargin     = 16;
constexpr int kRightMargin   = 16;
constexpr int kCornerRadius  = 8;

} // namespace

NoticeOverlay::NoticeOverlay(QWidget* trackTarget)
    : QWidget(trackTarget ? trackTarget->window() : nullptr,
              Qt::FramelessWindowHint
              | Qt::WindowStaysOnTopHint
              | Qt::Tool
              | Qt::WindowTransparentForInput
              | Qt::NoDropShadowWindowHint)
    , trackTarget_(trackTarget)
{
    // Pure presentation — never steals input or focus.
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setFocusPolicy(Qt::NoFocus);

    if (trackTarget_) {
        trackTarget_->installEventFilter(this);
        if (QWidget* top = trackTarget_->window(); top && top != trackTarget_) {
            top->installEventFilter(this);
        }
        syncToTarget();
    }
}

void NoticeOverlay::addNotice(const QString& id, Severity severity,
                               const QString& title, const QString& body) {
    for (auto& n : notices_) {
        if (n.id == id) {
            n.severity = severity;
            n.title = title;
            n.body = body;
            syncToTarget();
            update();
            return;
        }
    }
    notices_.append(Notice{id, severity, title, body});
    syncToTarget();
    update();
}

void NoticeOverlay::removeNotice(const QString& id) {
    for (int i = 0; i < notices_.size(); ++i) {
        if (notices_[i].id == id) {
            notices_.removeAt(i);
            if (notices_.isEmpty()) hide();
            else update();
            return;
        }
    }
}

void NoticeOverlay::clearNotices() {
    if (notices_.isEmpty()) return;
    notices_.clear();
    hide();
}

void NoticeOverlay::setOverlayEnabled(bool enabled) {
    if (enabled_ == enabled) return;
    enabled_ = enabled;
    if (!enabled_) {
        hide();
    } else {
        syncToTarget();
        update();
    }
}

void NoticeOverlay::setOpacityPercent(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    if (opacity_ == percent) return;
    opacity_ = percent;
    update();
}

void NoticeOverlay::syncToTarget() {
    if (!trackTarget_ || !enabled_ || notices_.isEmpty()) {
        hide();
        return;
    }
    if (!trackTarget_->isVisible()) {
        hide();
        return;
    }
    const QPoint topLeft = trackTarget_->mapToGlobal(QPoint(0, 0));
    setGeometry(QRect(topLeft, trackTarget_->size()));
    if (!isVisible()) show();
    raise();
}

bool NoticeOverlay::eventFilter(QObject* /*watched*/, QEvent* event) {
    switch (event->type()) {
        case QEvent::Move:
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::Hide:
        case QEvent::ParentChange:
        case QEvent::WindowStateChange:
            syncToTarget();
            break;
        default:
            break;
    }
    return false; // never consume
}

void NoticeOverlay::paintEvent(QPaintEvent* /*event*/) {
    if (!enabled_ || notices_.isEmpty()) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QFont titleFont(font().family(),
                           font().pointSize() > 0 ? font().pointSize() : 10,
                           QFont::DemiBold);
    const QFont bodyFont(font().family(),
                          font().pointSize() > 0 ? font().pointSize() : 10);
    const QFont glyphFont(font().family(),
                           (font().pointSize() > 0 ? font().pointSize() : 10) + 1,
                           QFont::Normal);

    const QFontMetrics titleFm(titleFont);
    const QFontMetrics bodyFm(bodyFont);
    const QFontMetrics glyphFm(glyphFont);

    const int width = qMin(kCardWidth, this->width() - 2 * kRightMargin);
    const int glyphWidth = glyphFm.horizontalAdvance(QStringLiteral("●"));
    const int textLeft = kCardPaddingX + glyphWidth + kGlyphGap;
    const int textWidth = width - textLeft - kCardPaddingX;
    const int x = this->width() - width - kRightMargin;
    int y = kTopMargin;

    const int bgAlpha = (255 * opacity_) / 100;
    const QColor cardBg(0x1B, 0x1D, 0x24, bgAlpha);
    const QColor titleColor(0xEC, 0xEC, 0xEF, qMin(255, bgAlpha + 30));
    const QColor bodyColor(0xB4, 0xB6, 0xBE, qMin(255, bgAlpha + 10));

    for (const auto& notice : notices_) {
        const SeverityStyle st = styleFor(notice.severity);

        // Wrap body text inside the available text column.
        const QRect bodyBounds = bodyFm.boundingRect(
            QRect(0, 0, textWidth, 10000),
            Qt::TextWordWrap, notice.body);
        const int titleH = titleFm.height();
        const int bodyH = notice.body.isEmpty() ? 0 : bodyBounds.height();
        const int gap = notice.body.isEmpty() ? 0 : 4;
        const int height = kCardPaddingY + titleH + gap + bodyH + kCardPaddingY;

        const QRectF cardRect(x, y, width, height);

        // Rounded panel with a thin severity-coloured stroke.
        QPainterPath path;
        path.addRoundedRect(cardRect, kCornerRadius, kCornerRadius);
        p.fillPath(path, cardBg);
        QPen border(QColor(st.accent.red(), st.accent.green(), st.accent.blue(),
                            qMin(255, bgAlpha + 20)));
        border.setWidthF(1.0);
        p.setPen(border);
        p.drawPath(path);

        // Severity glyph (left, accent colour, vertically aligned with title).
        p.setFont(glyphFont);
        p.setPen(st.accent);
        const int glyphY = y + kCardPaddingY;
        p.drawText(QRect(x + kCardPaddingX, glyphY, glyphWidth, titleH),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    st.glyph);

        // Title — accent-tinted, demi-bold.
        p.setFont(titleFont);
        p.setPen(st.accent);
        p.drawText(QRect(x + textLeft, y + kCardPaddingY, textWidth, titleH),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    titleFm.elidedText(notice.title, Qt::ElideRight, textWidth));

        // Body — dimmer, word-wrapped.
        if (!notice.body.isEmpty()) {
            p.setFont(bodyFont);
            p.setPen(bodyColor);
            p.drawText(QRect(x + textLeft, y + kCardPaddingY + titleH + gap,
                              textWidth, bodyH),
                        Qt::TextWordWrap, notice.body);
        }
        // Re-use the title colour for the next card — done.
        (void)titleColor;

        y += height + kCardSpacing;
    }
}

} // namespace microbotica::viewport
