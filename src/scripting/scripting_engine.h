#pragma once

#include <string>
#include <functional>

namespace microbotica::scripting {

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

    /// Check if initialized.
    bool isInitialized() const { return initialized_; }

private:
    bool initialized_ = false;
};

} // namespace microbotica::scripting
