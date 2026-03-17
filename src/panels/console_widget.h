#pragma once

#include <QDockWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <string>

#include "core/component_meta.h"
#include "core/stability.h"

namespace microbotica::scripting {
class ScriptingEngine;
} // namespace microbotica::scripting

namespace microbotica::panels {

/// Dock panel with a Python console (output + input line).
///
/// MBCA-COMP-041
///
/// Submits user input to ScriptingEngine::execute() and displays
/// stdout/stderr in the read-only output pane.
class ConsoleWidget : public QDockWidget {
    Q_OBJECT

public:
    explicit ConsoleWidget(scripting::ScriptingEngine& engine,
                           QWidget* parent = nullptr);
    ~ConsoleWidget() override = default;

    static const core::ComponentMeta& interfaceMeta() {
        static const core::ComponentMeta meta{
            .component_id      = "MBCA-COMP-041",
            .component_version = "1.0.0",
            .stability         = core::StabilityLevel::Experimental,
            .description       = "Embedded Python console dock widget with stdout capture.",
            .preconditions     = {"ScriptingEngine has been initialized (py::scoped_interpreter alive)"},
            .postconditions    = {"After execute, stdout/stderr are appended to the output pane"},
            .invariants        = {"Output pane is always read-only"},
            .design_rationale  = "Provides interactive scripting access to the USD scene "
                                 "for rapid prototyping and debugging during Phase 0.",
            .assumptions       = {"Only one ConsoleWidget instance exists",
                                  "ScriptingEngine outlives this widget"},
            .limitations       = {"No command history or tab completion",
                                  "No syntax highlighting"},
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

private slots:
    void onInputSubmitted();

private:
    scripting::ScriptingEngine& engine_;
    QPlainTextEdit* outputPane_ = nullptr;
    QLineEdit* inputLine_ = nullptr;
};

} // namespace microbotica::panels
