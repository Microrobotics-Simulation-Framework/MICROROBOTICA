#pragma once

#include <QDockWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include "panels/ansi_parser.h"

namespace microbotica::panels {

/// Dock panel displaying MIME runner subprocess stdout/stderr.
///
/// Separate from ConsoleWidget (Python REPL). This panel shows
/// the output of the QProcess running `python3 -m mime.runner`.
///
/// Features:
/// - ANSI color code rendering (JAX, MADDENING colored output)
/// - Auto-scroll with "Pin" toggle (scroll lock)
/// - Clear button
/// - Source-tagged output ([local] prefix)
class MimeConsolePanel : public QDockWidget {
    Q_OBJECT

public:
    explicit MimeConsolePanel(QWidget* parent = nullptr);

    /// Append stdout from the MIME process.
    void appendStdout(const QByteArray& data);

    /// Append stderr from the MIME process (displayed in red).
    void appendStderr(const QByteArray& data);

    /// Clear all output.
    void clear();

private:
    QPlainTextEdit* output_ = nullptr;
    QPushButton* pinButton_ = nullptr;
    AnsiParser stdoutParser_;
    AnsiParser stderrParser_;
    bool pinned_ = false;
};

} // namespace microbotica::panels
