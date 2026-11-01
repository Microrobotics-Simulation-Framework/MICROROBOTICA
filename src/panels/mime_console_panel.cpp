#include "panels/mime_console_panel.h"
#include "panels/mime_error_html.h"

#include <QHBoxLayout>
#include <QScrollBar>
#include <QTextCursor>
#include <QToolBar>
#include <QVBoxLayout>

#include "core/stability.h"

namespace microbotica::panels {

MimeConsolePanel::MimeConsolePanel(QWidget* parent)
    : QDockWidget(tr("MIME Output"), parent)
{
    MBCA_EXPERIMENTAL_WARN("MimeConsolePanel");
    setObjectName("MimeConsolePanel");

    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Toolbar
    auto* toolbar = new QToolBar(container);
    toolbar->setIconSize(QSize(16, 16));

    auto* clearAction = toolbar->addAction(tr("Clear"));
    connect(clearAction, &QAction::triggered, this, &MimeConsolePanel::clear);

    pinButton_ = new QPushButton(tr("Auto-scroll"), toolbar);
    pinButton_->setCheckable(true);
    pinButton_->setChecked(true);
    pinButton_->setToolTip(tr("Toggle auto-scroll to bottom on new output"));
    connect(pinButton_, &QPushButton::toggled, this, [this](bool checked) {
        pinned_ = !checked;
        pinButton_->setText(checked ? tr("Auto-scroll") : tr("Pinned"));
    });
    toolbar->addWidget(pinButton_);

    layout->addWidget(toolbar);

    // Output text. A browser rather than a plain text edit, so an error's
    // documentation page is a link that opens.
    output_ = new QTextBrowser(container);
    output_->setReadOnly(true);
    output_->setOpenExternalLinks(true);
    output_->document()->setMaximumBlockCount(10000);
    output_->setFont(QFont("Monospace", 9));

    // Detect user scroll-up as implicit pin
    connect(output_->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int value) {
        if (value < output_->verticalScrollBar()->maximum()) {
            if (!pinned_) {
                pinned_ = true;
                pinButton_->setChecked(false);
            }
        }
    });

    layout->addWidget(output_);
    setWidget(container);

    // A partial stderr line (a progress line with no newline yet) is shown
    // after a moment rather than held until the next newline.
    flushTimer_ = new QTimer(this);
    flushTimer_->setSingleShot(true);
    flushTimer_->setInterval(300);
    connect(flushTimer_, &QTimer::timeout, this, [this]() {
        if (!stderrPartial_.empty()) {
            showFed(errorReader_.feed(stderrPartial_));
            stderrPartial_.clear();
        }
    });
}

void MimeConsolePanel::appendHtmlBlock(const QString& html) {
    QTextCursor cursor(output_->document());
    cursor.movePosition(QTextCursor::End);
    if (!output_->document()->isEmpty()) {
        cursor.insertBlock();
    }
    cursor.insertHtml(html);
}

void MimeConsolePanel::scrollIfFollowing() {
    if (!pinned_) {
        output_->verticalScrollBar()->setValue(output_->verticalScrollBar()->maximum());
    }
}

void MimeConsolePanel::appendStdout(const QByteArray& data) {
    std::string text = data.toStdString();
    appendHtmlBlock("<span style=\"color:#888\">[local]</span> " + stdoutParser_.toHtml(text));
    scrollIfFollowing();
}

void MimeConsolePanel::showFed(const core::MimeErrorTextReader::Fed& fed) {
    for (const auto& line : fed.plain) {
        appendHtmlBlock("<span style=\"color:#c00\">[local]</span> " + stderrParser_.toHtml(line));
    }
    for (const auto& report : fed.reports) {
        appendHtmlBlock("<span style=\"color:#c00\">[local]</span> " + mimeErrorReportHtml(report));
        reports_.push_back(report);
    }
    scrollIfFollowing();
}

void MimeConsolePanel::appendStderr(const QByteArray& data) {
    stderrPartial_ += data.toStdString();
    std::size_t newline;
    while ((newline = stderrPartial_.find('\n')) != std::string::npos) {
        std::string line = stderrPartial_.substr(0, newline);
        stderrPartial_.erase(0, newline + 1);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        showFed(errorReader_.feed(line));
    }
    if (!stderrPartial_.empty()) {
        flushTimer_->start();
    }
}

void MimeConsolePanel::appendRefusals(const QString& headline, const std::vector<core::MimeRefusal>& refusals) {
    core::MimeErrorReport report;
    report.headline = headline.toStdString();
    report.refusals = refusals;
    appendHtmlBlock("<span style=\"color:#c00\">[reply]</span> " + mimeErrorReportHtml(report));
    reports_.push_back(std::move(report));
    scrollIfFollowing();
}

void MimeConsolePanel::flush() {
    flushTimer_->stop();
    if (!stderrPartial_.empty()) {
        showFed(errorReader_.feed(stderrPartial_));
        stderrPartial_.clear();
    }
    showFed(errorReader_.finish());
}

void MimeConsolePanel::clear() {
    output_->clear();
    stdoutParser_.reset();
    stderrParser_.reset();
    flushTimer_->stop();
    stderrPartial_.clear();
    errorReader_ = core::MimeErrorTextReader{};
    reports_.clear();
}

} // namespace microbotica::panels
