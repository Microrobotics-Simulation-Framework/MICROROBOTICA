#pragma once

#include <QDockWidget>
#include <QPushButton>
#include <QTextBrowser>
#include <QTimer>

#include <string>
#include <vector>

#include "core/component_meta.h"
#include "core/mime_error.h"
#include "panels/ansi_parser.h"

namespace microbotica::panels {

/// Dock panel displaying a MIME process's stdout/stderr, and MIME's coded errors in their shape.
///
/// Separate from ConsoleWidget (Python REPL). This panel shows the output of the MIME process MICROROBOTICA runs
/// (`mime serve`), and the refusals of the live protocol's replies.
///
/// Features:
/// - ANSI color code rendering (JAX, MADDENING colored output)
/// - MIME's coded errors (MIME 0.3, docs/errors.md) shown in their shape: each reason's class and code, what
///   happened, why, what to do, where, and its documentation page as a link, whether they arrive as text on stderr
///   or as a reply's `errors`
/// - Auto-scroll with "Pin" toggle (scroll lock)
/// - Clear button
/// - Source-tagged output ([local] prefix)
///
/// MBCA-COMP-042
class MimeConsolePanel : public QDockWidget {
    Q_OBJECT

public:
    static const core::ComponentMeta& meta() {
        static const core::ComponentMeta m{
            .component_id      = "MBCA-COMP-042",
            .component_version = "1.1.0",
            .stability         = core::StabilityLevel::Experimental,
            .description       = "Console of the MIME process's output, with MIME's coded errors shown in "
                                 "their shape (class, code, message, why, what to do, where, page).",
            .preconditions     = {},
            .postconditions    = {"Every line of output is shown, as it came or as part of an error in its shape",
                                  "Every part of a coded error MIME printed or replied is shown"},
            .invariants        = {"Output text is HTML-escaped before display"},
            .design_rationale  = "MIME 0.3 gives every error a code, a class and an action; showing them in that "
                                 "shape tells the user what to do without reading a traceback.",
            .assumptions       = {"MIME prints coded errors in the shape of its docs/errors.md (MIME 0.3)"},
            .limitations       = {"A partial stderr line is shown after 300 ms, or at flush()",
                                  "Lines outside MIME's shape are shown as they came"},
            .validated_regimes = {},
            .hazard_hints      = {
                "An error printed in a shape MIME no longer uses is shown as plain lines, not lost.",
            },
            .references        = {},
            .deprecation_notice = std::nullopt,
        };
        return m;
    }

    explicit MimeConsolePanel(QWidget* parent = nullptr);

    /// Append stdout from the MIME process.
    void appendStdout(const QByteArray& data);

    /// Append stderr from the MIME process (displayed in red). Read line by line: MIME's coded errors in it are shown
    /// in their shape.
    void appendStderr(const QByteArray& data);

    /// Show a live-protocol reply's refusals (its `errors`), with what was refused (`headline`, e.g. "set is
    /// refused").
    void appendRefusals(const QString& headline, const std::vector<core::MimeRefusal>& refusals);

    /// Show what stderr holds back (a partial line, an error still open): call when the process ends.
    void flush();

    /// Clear all output.
    void clear();

    /// Every MIME error shown since the last clear(), newest last (for the scripting console and tests).
    const std::vector<core::MimeErrorReport>& reports() const { return reports_; }

private:
    void appendHtmlBlock(const QString& html);
    void showFed(const core::MimeErrorTextReader::Fed& fed);
    void scrollIfFollowing();

    QTextBrowser* output_ = nullptr;
    QPushButton* pinButton_ = nullptr;
    AnsiParser stdoutParser_;
    AnsiParser stderrParser_;
    bool pinned_ = false;

    // stderr arrives in chunks that need not end at a line: the partial line waits here, briefly (flushTimer_).
    std::string stderrPartial_;
    core::MimeErrorTextReader errorReader_;
    QTimer* flushTimer_ = nullptr;
    std::vector<core::MimeErrorReport> reports_;
};

} // namespace microbotica::panels
