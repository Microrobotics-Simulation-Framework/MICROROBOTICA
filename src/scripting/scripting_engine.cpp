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

} // namespace microbotica::scripting
