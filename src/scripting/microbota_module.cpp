#include "scripting/pybind11_guard.h"
#include "core/component_meta.h"
#include "core/types.h"
#include "core/result_frame.h"

// Suppress visibility warnings from pybind11 types in std::function
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"

namespace microbotica::scripting {

/// Global context set by Application before Python is initialized.
/// Allows the embedded module to access the running application state.
struct ModuleContext {
    // Scene proxy callbacks
    std::function<std::vector<std::string>()> getPrimPaths;
    std::function<py::object(const std::string&, const std::string&)> getAttribute;
    std::function<void(const std::string&, const std::string&, py::object)> setAttribute;

    // Sim proxy callbacks
    std::function<double()> getCurrentTime;
    std::function<py::dict()> getCurrentFrame;
    std::function<void(py::dict)> sendParameters;
    std::function<std::string()> getStatus;

    bool simRunning = false;
};

ModuleContext& moduleContext() {
    static ModuleContext ctx;
    return ctx;
}

} // namespace microbotica::scripting

#pragma GCC diagnostic pop

// Proxy types must be defined at file scope for pybind11
struct SceneProxy {};
struct SimProxy {};

/// MBCA-COMP-040: microrobotica Python embedded module
PYBIND11_EMBEDDED_MODULE(microrobotica, m) {
    using namespace microbotica::scripting;

    m.doc() = "MICROBOTICA embedded Python module — scene and simulation access";

    // Scene proxy
    py::class_<SceneProxy>(m, "SceneProxy")
        .def("prim_paths", [](const SceneProxy&) {
            auto& ctx = moduleContext();
            if (ctx.getPrimPaths) return ctx.getPrimPaths();
            return std::vector<std::string>{};
        })
        .def("get_attribute", [](const SceneProxy&,
                                  const std::string& prim, const std::string& attr) {
            auto& ctx = moduleContext();
            if (ctx.getAttribute) return ctx.getAttribute(prim, attr);
            return py::object(py::none());
        })
        .def("set_attribute", [](const SceneProxy&,
                                  const std::string& prim, const std::string& attr,
                                  py::object value) {
            auto& ctx = moduleContext();
            if (ctx.simRunning) {
                throw std::runtime_error(
                    "Cannot modify scene attributes while simulation is Running. "
                    "Stop or pause the simulation first.");
            }
            if (ctx.setAttribute) ctx.setAttribute(prim, attr, std::move(value));
        });

    // Sim proxy
    py::class_<SimProxy>(m, "SimProxy")
        .def("current_time", [](const SimProxy&) {
            auto& ctx = moduleContext();
            if (ctx.getCurrentTime) return ctx.getCurrentTime();
            return 0.0;
        })
        .def("current_frame", [](const SimProxy&) {
            auto& ctx = moduleContext();
            if (ctx.getCurrentFrame) return ctx.getCurrentFrame();
            return py::dict();
        })
        .def("send_parameters", [](const SimProxy&, py::dict params) {
            auto& ctx = moduleContext();
            for (auto item : params) {
                if (!py::isinstance<py::str>(item.first)) {
                    throw py::type_error("Parameter keys must be strings");
                }
            }
            if (ctx.sendParameters) ctx.sendParameters(std::move(params));
        })
        .def("status", [](const SimProxy&) {
            auto& ctx = moduleContext();
            if (ctx.getStatus) return ctx.getStatus();
            return std::string("idle");
        });

    // Module-level accessors
    m.def("scene", []() { return SceneProxy{}; });
    m.def("sim", []() { return SimProxy{}; });
}
