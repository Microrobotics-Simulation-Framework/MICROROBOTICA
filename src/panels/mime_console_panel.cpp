#include "panels/mime_console_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QToolBar>

namespace microbotica::panels {

MimeConsolePanel::MimeConsolePanel(QWidget* parent)
    : QDockWidget(tr("MIME Output"), parent)
{
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

    // Output text
    output_ = new QPlainTextEdit(container);
    output_->setReadOnly(true);
    output_->setMaximumBlockCount(10000);
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
}

void MimeConsolePanel::appendStdout(const QByteArray& data) {
    std::string text = data.toStdString();
    QString html = "<span style=\"color:#888\">[local]</span> " + stdoutParser_.toHtml(text);
    output_->appendHtml(html);

    if (!pinned_) {
        output_->verticalScrollBar()->setValue(output_->verticalScrollBar()->maximum());
    }
}

void MimeConsolePanel::appendStderr(const QByteArray& data) {
    std::string text = data.toStdString();
    QString html = "<span style=\"color:#c00\">[local]</span> " + stderrParser_.toHtml(text);
    output_->appendHtml(html);

    if (!pinned_) {
        output_->verticalScrollBar()->setValue(output_->verticalScrollBar()->maximum());
    }
}

void MimeConsolePanel::clear() {
    output_->clear();
    stdoutParser_.reset();
    stderrParser_.reset();
}

} // namespace microbotica::panels
