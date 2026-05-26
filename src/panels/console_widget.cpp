#include "panels/console_widget.h"

#include <QFont>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QTextBlock>
#include <QTextCursor>
#include <QVBoxLayout>
#include <QWidget>

#include "panels/python_highlighter.h"
#include "scripting/scripting_engine.h"

namespace microbotica::panels {

// ── ConsoleInputEdit ────────────────────────────────────────────────────────

ConsoleInputEdit::ConsoleInputEdit(scripting::ScriptingEngine& engine,
                                    QWidget* parent)
    : QPlainTextEdit(parent)
    , engine_(engine)
{
    setFont(QFont("Monospace", 10));
    setTabStopDistance(
        QFontMetricsF(font()).horizontalAdvance(' ') * 4);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setPlaceholderText(tr(">>> Python — Enter to run, Shift+Enter for newline, "
                           "Up/Down for history, Tab to complete"));

    // Modest fixed height that grows naturally if the user types more lines.
    setMaximumBlockCount(0);
    const int lineH = QFontMetricsF(font()).lineSpacing();
    setMinimumHeight(lineH * 2 + 8);
    setMaximumHeight(lineH * 10 + 8);

    highlighter_ = new PythonHighlighter(document());
}

void ConsoleInputEdit::setHighlightTheme(const QString& themeName) {
    if (highlighter_) highlighter_->setThemeByName(themeName);
}

void ConsoleInputEdit::keyPressEvent(QKeyEvent* event) {
    const auto key = event->key();
    const auto mods = event->modifiers();
    const bool isEnter = (key == Qt::Key_Return || key == Qt::Key_Enter);

    if (isEnter && (mods & Qt::ShiftModifier)) {
        handleEnter(/*forceNewline=*/true);
        return;
    }
    if (isEnter && (mods & (Qt::ControlModifier | Qt::AltModifier
                             | Qt::MetaModifier)) == 0) {
        handleEnter(/*forceNewline=*/false);
        return;
    }

    if (key == Qt::Key_Up && mods == Qt::NoModifier) {
        const QTextCursor cur = textCursor();
        if (cur.blockNumber() == 0) {
            handleUpArrow();
            return;
        }
    }
    if (key == Qt::Key_Down && mods == Qt::NoModifier) {
        const QTextCursor cur = textCursor();
        if (cur.blockNumber() == document()->blockCount() - 1) {
            handleDownArrow();
            return;
        }
    }

    if (key == Qt::Key_Tab && mods == Qt::NoModifier) {
        handleTab();
        return;
    }

    QPlainTextEdit::keyPressEvent(event);
}

void ConsoleInputEdit::handleEnter(bool forceNewline) {
    if (forceNewline) {
        insertPlainText("\n");
        return;
    }

    const QString text = toPlainText();
    if (text.trimmed().isEmpty()) {
        return; // Enter on blank input — no-op.
    }

    const auto status = engine_.checkComplete(text.toStdString());
    if (status == scripting::CompletionStatus::Incomplete) {
        // Re-indent: copy the leading whitespace of the current line, plus
        // one tab if it ends with a colon (block opener).
        const QTextCursor cur = textCursor();
        const QString line = cur.block().text();
        QString indent;
        for (QChar ch : line) {
            if (ch == QLatin1Char(' ') || ch == QLatin1Char('\t')) indent += ch;
            else break;
        }
        if (line.trimmed().endsWith(QLatin1Char(':'))) {
            indent += QStringLiteral("    ");
        }
        insertPlainText("\n" + indent);
        return;
    }

    // Complete (or definitively invalid — submit and let Python explain).
    pushHistory(text);
    Q_EMIT submitted(text);
    clear();
}

void ConsoleInputEdit::handleUpArrow() {
    if (history_.isEmpty()) return;
    if (historyIdx_ == -1) {
        liveBuffer_ = toPlainText();
        historyIdx_ = history_.size() - 1;
    } else if (historyIdx_ > 0) {
        --historyIdx_;
    } else {
        return; // already at oldest
    }
    setPlainText(history_[historyIdx_]);
    QTextCursor cur = textCursor();
    cur.movePosition(QTextCursor::End);
    setTextCursor(cur);
}

void ConsoleInputEdit::handleDownArrow() {
    if (historyIdx_ == -1) return;
    if (historyIdx_ + 1 >= history_.size()) {
        historyIdx_ = -1;
        setPlainText(liveBuffer_);
    } else {
        ++historyIdx_;
        setPlainText(history_[historyIdx_]);
    }
    QTextCursor cur = textCursor();
    cur.movePosition(QTextCursor::End);
    setTextCursor(cur);
}

void ConsoleInputEdit::handleTab() {
    int prefixStart = 0;
    const QString prefix = currentIdentifierPrefix(prefixStart);
    if (prefix.isEmpty()) {
        // No identifier under cursor — fall back to inserting indentation.
        insertPlainText(QStringLiteral("    "));
        return;
    }

    const auto candidates = engine_.completions(prefix.toStdString());
    QStringList unique;
    unique.reserve(static_cast<int>(candidates.size()));
    for (const auto& c : candidates) {
        const QString s = QString::fromStdString(c);
        if (!unique.contains(s)) unique.append(s);
    }
    if (unique.isEmpty()) return;

    QTextCursor cur = textCursor();
    cur.setPosition(prefixStart, QTextCursor::MoveAnchor);
    cur.setPosition(prefixStart + prefix.length(), QTextCursor::KeepAnchor);

    if (unique.size() == 1) {
        cur.insertText(unique.first());
        return;
    }

    const QString lcp = longestCommonPrefix(unique);
    if (lcp.length() > prefix.length()) {
        cur.insertText(lcp);
    }
    Q_EMIT completionListProduced(unique);
}

void ConsoleInputEdit::pushHistory(const QString& entry) {
    if (!entry.trimmed().isEmpty()
        && (history_.isEmpty() || history_.last() != entry)) {
        history_.append(entry);
        // Cap to avoid unbounded growth.
        if (history_.size() > 500) history_.removeFirst();
    }
    historyIdx_ = -1;
    liveBuffer_.clear();
}

QString ConsoleInputEdit::currentIdentifierPrefix(int& outStart) const {
    const QString full = toPlainText();
    const int cursor = textCursor().position();
    int i = cursor;
    while (i > 0) {
        const QChar ch = full[i - 1];
        if (ch.isLetterOrNumber() || ch == QLatin1Char('_')
            || ch == QLatin1Char('.')) {
            --i;
        } else {
            break;
        }
    }
    outStart = i;
    return full.mid(i, cursor - i);
}

QString ConsoleInputEdit::longestCommonPrefix(const QStringList& items) {
    if (items.isEmpty()) return {};
    QString p = items.first();
    for (int i = 1; i < items.size(); ++i) {
        const QString& s = items[i];
        int k = 0;
        const int lim = qMin(p.length(), s.length());
        while (k < lim && p[k] == s[k]) ++k;
        p.truncate(k);
        if (p.isEmpty()) break;
    }
    return p;
}

// ── ConsoleWidget ───────────────────────────────────────────────────────────

ConsoleWidget::ConsoleWidget(scripting::ScriptingEngine& engine,
                               QWidget* parent)
    : QDockWidget(tr("Console"), parent)
    , engine_(engine)
{
    MBCA_EXPERIMENTAL_WARN("ConsoleWidget");

    setObjectName("ConsoleWidget");

    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);

    outputPane_ = new QPlainTextEdit(container);
    outputPane_->setReadOnly(true);
    outputPane_->setFont(QFont("Monospace", 10));
    outputPane_->setPlaceholderText(tr("Python console output..."));

    inputEdit_ = new ConsoleInputEdit(engine_, container);

    layout->addWidget(outputPane_, 1);
    layout->addWidget(inputEdit_, 0);
    layout->setContentsMargins(2, 2, 2, 2);

    container->setLayout(layout);
    setWidget(container);

    connect(inputEdit_, &ConsoleInputEdit::submitted,
            this, &ConsoleWidget::onInputSubmitted);
    connect(inputEdit_, &ConsoleInputEdit::completionListProduced,
            this, &ConsoleWidget::onCompletionList);
}

void ConsoleWidget::appendOutput(const QString& text) {
    outputPane_->appendPlainText(text);
    outputPane_->ensureCursorVisible();
}

void ConsoleWidget::setHighlightTheme(const QString& themeName) {
    if (inputEdit_) inputEdit_->setHighlightTheme(themeName);
}

void ConsoleWidget::onInputSubmitted(const QString& code) {
    // Echo with `>>> ` on the first line and `... ` on continuation lines.
    const QStringList lines = code.split(QLatin1Char('\n'));
    for (int i = 0; i < lines.size(); ++i) {
        appendOutput(QString(i == 0 ? ">>> %1" : "... %1").arg(lines[i]));
    }

    std::string stdoutStr, stderrStr;
    const bool ok = engine_.execute(code.toStdString(), stdoutStr, stderrStr);

    if (!stdoutStr.empty()) {
        // appendPlainText already adds a newline; trim any trailing one.
        QString s = QString::fromStdString(stdoutStr);
        if (s.endsWith(QLatin1Char('\n'))) s.chop(1);
        appendOutput(s);
    }
    if (!stderrStr.empty()) {
        appendOutput(QString::fromStdString(stderrStr));
    }
    if (!ok && stderrStr.empty()) {
        appendOutput(tr("(execution failed)"));
    }
}

void ConsoleWidget::onCompletionList(const QStringList& candidates) {
    appendOutput(QString("[%1 completions] %2")
                     .arg(candidates.size())
                     .arg(candidates.join(QStringLiteral("  "))));
}

} // namespace microbotica::panels
