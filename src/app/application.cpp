#include "scripting/pybind11_guard.h"
#include "app/application.h"
#include "viewport/storm_surface_format.h"

// Suppress pybind11 visibility warnings
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"

namespace microbotica::app {

struct Application::Impl {
    py::scoped_interpreter interpreter{};
};

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
{
    MBCA_EXPERIMENTAL_WARN("Application");

    setApplicationName("MICROBOTICA");
    setApplicationVersion("0.1.0");
    setOrganizationName("MICROBOTICA");

    // The default surface format, before any OpenGL window or context is
    // created: what Storm needs (4.5 Compatibility, with the deprecated
    // functions), so a context made without a format of its own can draw it.
    // A Core default here once made every such context one Storm cannot use.
    QSurfaceFormat::setDefaultFormat(viewport::stormSurfaceFormat(0));

    // Initialize the embedded Python interpreter
    pimpl_ = new Impl();
}

Application::~Application() {
    delete pimpl_;
}

} // namespace microbotica::app

#pragma GCC diagnostic pop
