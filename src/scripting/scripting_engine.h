#pragma once

#include <string>
#include <vector>
#include <functional>

namespace microbotica::scripting {

/// Result of asking Python whether a piece of source is a complete
/// statement — drives the interactive console's multi-line input.
enum class CompletionStatus {
    Complete,    ///< Ready to run.
    Incomplete,  ///< The user must keep typing (e.g. `for x in range(3):`).
    Invalid,     ///< Definitively a syntax error. Submit anyway and let the runtime report.
};

/// Embedded Python scripting engine.
///
/// Does NOT own the py::scoped_interpreter — that lives in Application.
/// This class manages sys.path, module imports, and code execution.
class ScriptingEngine {
public:
    ScriptingEngine() = default;

    /// Initialize the scripting environment (set sys.path, import microrobotica).
    /// Must be called after py::scoped_interpreter is created.
    bool initialize();

    /// Execute Python code. Returns true on success.
    /// Captures stdout/stderr into the provided strings.
    bool execute(const std::string& code,
                 std::string& stdoutStr,
                 std::string& stderrStr);

    /// Ask CPython's `code.compile_command` whether `source` is a complete
    /// statement. Used by the console to decide whether Enter should submit
    /// or insert a newline (true interactive REPL behaviour).
    CompletionStatus checkComplete(const std::string& source);

    /// Return the tab-completion candidates for `prefix` using
    /// `rlcompleter.Completer` against the engine's globals. Empty on any
    /// failure — completion is purely best-effort.
    std::vector<std::string> completions(const std::string& prefix);

    /// Check if initialized.
    bool isInitialized() const { return initialized_; }

private:
    bool initialized_ = false;
};

} // namespace microbotica::scripting
