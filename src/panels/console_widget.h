#pragma once

#include <QDockWidget>
#include <QPlainTextEdit>
#include <QStringList>
#include <string>

#include "core/component_meta.h"
#include "core/stability.h"

namespace microbotica::scripting {
class ScriptingEngine;
} // namespace microbotica::scripting

namespace microbotica::panels {

class PythonHighlighter;

/// Multi-line Python input field for the embedded console.
///
/// Behaviours:
/// - Enter submits if the buffer is a complete Python statement (via
///   CPython's `code.compile_command`); otherwise inserts a newline with
///   the previous line's leading indent. So `for x in range(3):` keeps
///   you typing — no spurious SyntaxError.
/// - Shift+Enter always inserts a newline (manual override).
/// - Up / Down navigate command history when the cursor is on the first
///   or last line of the buffer.
/// - Tab triggers rlcompleter-based completion of the identifier
///   immediately to the left of the cursor; a single match is inserted
///   inline, multiple matches are echoed once to the output pane.
class ConsoleInputEdit : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit ConsoleInputEdit(scripting::ScriptingEngine& engine,
                              QWidget* parent = nullptr);

    /// Apply a PythonHighlighter preset by name.
    void setHighlightTheme(const QString& themeName);

Q_SIGNALS:
    /// Emitted when the user presses Enter on a complete (or invalid)
    /// statement. `code` is the full multi-line buffer; the receiver is
    /// responsible for echoing and running it.
    void submitted(const QString& code);

    /// Emitted when Tab-completion produced multiple matches — the
    /// console echoes them into its output pane.
    void completionListProduced(const QStringList& candidates);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void handleEnter(bool forceNewline);
    void handleUpArrow();
    void handleDownArrow();
    void handleTab();
    void pushHistory(const QString& entry);
    QString currentIdentifierPrefix(int& outStart) const;
    static QString longestCommonPrefix(const QStringList& items);

    scripting::ScriptingEngine& engine_;
    PythonHighlighter* highlighter_ = nullptr;

    QStringList history_;
    int historyIdx_ = -1;     ///< -1 = past the end (live buffer)
    QString liveBuffer_;      ///< buffer to restore when scrolling past newest
};

/// Dock panel with a Python console (read-only output pane + multi-line
/// input editor).
///
/// MBCA-COMP-041
class ConsoleWidget : public QDockWidget {
    Q_OBJECT

public:
    explicit ConsoleWidget(scripting::ScriptingEngine& engine,
                           QWidget* parent = nullptr);
    ~ConsoleWidget() override = default;

    static const core::ComponentMeta& interfaceMeta() {
        static const core::ComponentMeta meta{
            .component_id      = "MBCA-COMP-041",
            .component_version = "1.1.0",
            .stability         = core::StabilityLevel::Experimental,
            .description       = "Embedded Python REPL with history, multi-line input, "
                                 "Tab completion, and themed syntax highlighting.",
            .preconditions     = {"ScriptingEngine has been initialized (py::scoped_interpreter alive)"},
            .postconditions    = {"After execute, stdout/stderr are appended to the output pane"},
            .invariants        = {"Output pane is always read-only"},
            .design_rationale  = "Provides interactive scripting access to the USD scene "
                                 "for rapid prototyping and debugging during Phase 0.",
            .assumptions       = {"Only one ConsoleWidget instance exists",
                                  "ScriptingEngine outlives this widget"},
            .limitations       = {"Single-shot py::exec — no expression auto-print yet",
                                  "Completion is best-effort (rlcompleter scope)"},
            .validated_regimes = {},
            .hazard_hints      = {
                "Arbitrary Python execution — no sandboxing in Phase 0.",
            },
            .references        = {},
            .deprecation_notice = std::nullopt,
        };
        return meta;
    }

    /// Append text to the output pane.
    void appendOutput(const QString& text);

    /// Apply a PythonHighlighter preset by name (also restyles the input).
    void setHighlightTheme(const QString& themeName);

private Q_SLOTS:
    void onInputSubmitted(const QString& code);
    void onCompletionList(const QStringList& candidates);

private:
    scripting::ScriptingEngine& engine_;
    QPlainTextEdit* outputPane_ = nullptr;
    ConsoleInputEdit* inputEdit_ = nullptr;
};

} // namespace microbotica::panels
