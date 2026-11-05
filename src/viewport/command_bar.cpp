#include "viewport/command_bar.h"

#include <QClipboard>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace microbotica::viewport {

CommandBar::CommandBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("commandBar"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(4);

    auto* header = new QHBoxLayout();
    caption_ = new QLabel(this);
    caption_->setObjectName(QStringLiteral("commandBarCaption"));
    caption_->setWordWrap(true);
    caption_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    header->addWidget(caption_, 1);
    auto* dismiss = new QToolButton(this);
    dismiss->setObjectName(QStringLiteral("commandBarDismiss"));
    dismiss->setText(QStringLiteral("✕"));
    dismiss->setToolTip(tr("Hide until the next notice"));
    dismiss->setAutoRaise(true);
    connect(dismiss, &QToolButton::clicked, this, &CommandBar::hide);
    header->addWidget(dismiss, 0, Qt::AlignTop);
    layout->addLayout(header);

    rows_ = new QVBoxLayout();
    rows_->setSpacing(4);
    layout->addLayout(rows_);
    hide();
}

QString CommandBar::caption() const {
    return caption_->text();
}

void CommandBar::setCommands(const QString& caption, const QStringList& commands) {
    // Out of the bar at once, deleted later: a row's own button may be what called this.
    for (QWidget* row : rowWidgets_) {
        rows_->removeWidget(row);
        row->hide();
        row->setParent(nullptr);
        row->deleteLater();
    }
    rowWidgets_.clear();
    commands_.clear();
    for (const QString& command : commands) {
        const QString text = command.trimmed();
        if (!text.isEmpty() && !commands_.contains(text)) {
            commands_ << text;
        }
    }
    caption_->setText(caption);
    if (commands_.isEmpty()) {
        hide();
        return;
    }
    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    for (int i = 0; i < commands_.size(); ++i) {
        auto* row = new QWidget(this);
        auto* line = new QHBoxLayout(row);
        line->setContentsMargins(0, 0, 0, 0);
        auto* field = new QLineEdit(commands_[i], row);
        field->setObjectName(QStringLiteral("commandBarField"));
        field->setReadOnly(true);
        field->setFont(mono);
        field->setCursorPosition(0);
        field->setToolTip(commands_[i]);
        line->addWidget(field, 1);
        auto* button = new QPushButton(tr("Copy"), row);
        button->setObjectName(QStringLiteral("commandBarCopy"));
        button->setToolTip(tr("Copy the command, to paste in a terminal"));
        connect(button, &QPushButton::clicked, this, [this, i]() { copy(i); });
        line->addWidget(button);
        rows_->addWidget(row);
        rowWidgets_ << row;
    }
    show();
}

void CommandBar::clear() {
    setCommands(QString(), {});
}

void CommandBar::copy(int index) {
    if (index < 0 || index >= commands_.size()) return;
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (!clipboard) return;
    clipboard->setText(commands_[index], QClipboard::Clipboard);
    if (clipboard->supportsSelection()) {
        clipboard->setText(commands_[index], QClipboard::Selection);
    }
    Q_EMIT copied(commands_[index]);
}

} // namespace microbotica::viewport
