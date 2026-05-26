#include "scripting/scripting_engine.h"
#include "scripting/pybind11_guard.h"
#include <spdlog/spdlog.h>

namespace microbotica::scripting {

bool ScriptingEngine::initialize() {
    try {
        py::module_::import("sys");

        // Try to import the embedded microrobotica module
        try {
            py::module_::import("microrobotica");
            spdlog::info("ScriptingEngine: microrobotica module loaded");
        } catch (const py::error_already_set& e) {
            spdlog::warn("ScriptingEngine: microrobotica module not available: {}", e.what());
        }

        initialized_ = true;
        return true;
    } catch (const std::exception& e) {
        spdlog::error("ScriptingEngine: Failed to initialize: {}", e.what());
        return false;
    }
}

bool ScriptingEngine::execute(const std::string& code,
                               std::string& stdoutStr,
                               std::string& stderrStr) {
    if (!initialized_) {
        stderrStr = "ScriptingEngine not initialized";
        return false;
    }

    try {
        // Redirect stdout/stderr
        py::module_ io = py::module_::import("io");
        py::object stringIO = io.attr("StringIO");
        py::object capturedOut = stringIO();
        py::object capturedErr = stringIO();

        py::module_ sys = py::module_::import("sys");
        py::object oldOut = sys.attr("stdout");
        py::object oldErr = sys.attr("stderr");

        sys.attr("stdout") = capturedOut;
        sys.attr("stderr") = capturedErr;

        try {
            py::exec(code);
        } catch (const py::error_already_set& e) {
            sys.attr("stdout") = oldOut;
            sys.attr("stderr") = oldErr;
            stderrStr = e.what();
            stdoutStr = capturedOut.attr("getvalue")().cast<std::string>();
            return false;
        }

        sys.attr("stdout") = oldOut;
        sys.attr("stderr") = oldErr;

        stdoutStr = capturedOut.attr("getvalue")().cast<std::string>();
        stderrStr = capturedErr.attr("getvalue")().cast<std::string>();

        return true;
    } catch (const std::exception& e) {
        stderrStr = e.what();
        return false;
    }
}

CompletionStatus ScriptingEngine::checkComplete(const std::string& source) {
    if (!initialized_) return CompletionStatus::Complete; // submit and fail loudly
    try {
        py::module_ codemod = py::module_::import("code");
        // compile_command returns:
        //   None  → incomplete
        //   code  → complete and valid
        //   raises SyntaxError → genuinely invalid
        try {
            py::object result =
                codemod.attr("compile_command")(source, "<console>", "single");
            return result.is_none() ? CompletionStatus::Incomplete
                                     : CompletionStatus::Complete;
        } catch (const py::error_already_set&) {
            return CompletionStatus::Invalid;
        }
    } catch (const std::exception&) {
        return CompletionStatus::Complete;
    }
}

std::vector<std::string> ScriptingEngine::completions(const std::string& prefix) {
    std::vector<std::string> out;
    if (!initialized_) return out;
    try {
        py::module_ rl = py::module_::import("rlcompleter");
        py::module_ builtins = py::module_::import("builtins");
        py::dict ns = py::module_::import("__main__").attr("__dict__");
        py::object completer = rl.attr("Completer")(ns);

        // Walk completer.complete(prefix, i) for i=0,1,... until None.
        for (int i = 0; i < 64; ++i) {
            py::object c = completer.attr("complete")(prefix, i);
            if (c.is_none()) break;
            out.push_back(c.cast<std::string>());
        }
    } catch (const py::error_already_set&) {
        out.clear();
    } catch (const std::exception&) {
        out.clear();
    }
    return out;
}

} // namespace microbotica::scripting
