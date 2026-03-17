#include "panels/console_widget.h"

#include <QVBoxLayout>
#include <QWidget>

#include "scripting/scripting_engine.h"

namespace microbotica::panels {

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
    outputPane_->setPlaceholderText(tr("Python console output..."));

    inputLine_ = new QLineEdit(container);
    inputLine_->setPlaceholderText(tr(">>> Enter Python command..."));

    layout->addWidget(outputPane_, 1);
    layout->addWidget(inputLine_, 0);
    layout->setContentsMargins(2, 2, 2, 2);

    container->setLayout(layout);
    setWidget(container);

    connect(inputLine_, &QLineEdit::returnPressed,
            this, &ConsoleWidget::onInputSubmitted);
}

void ConsoleWidget::appendOutput(const QString& text)
{
    outputPane_->appendPlainText(text);
    outputPane_->ensureCursorVisible();
}

void ConsoleWidget::onInputSubmitted()
{
    const QString input = inputLine_->text().trimmed();
    if (input.isEmpty()) return;

    // Echo the command
    appendOutput(QString(">>> %1").arg(input));
    inputLine_->clear();

    // Execute via ScriptingEngine
    std::string stdoutStr, stderrStr;
    const bool ok = engine_.execute(input.toStdString(), stdoutStr, stderrStr);

    if (!stdoutStr.empty()) {
        appendOutput(QString::fromStdString(stdoutStr));
    }
    if (!stderrStr.empty()) {
        appendOutput(QString("Error: %1").arg(QString::fromStdString(stderrStr)));
    }
    if (!ok && stderrStr.empty()) {
        appendOutput(tr("(execution failed)"));
    }
}

} // namespace microbotica::panels
