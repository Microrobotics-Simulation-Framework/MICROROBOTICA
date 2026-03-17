#include "scripting/pybind11_guard.h"
#include "app/application.h"

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

    // Set OpenGL surface format before any QOpenGLWidget is created
    QSurfaceFormat format;
    format.setVersion(4, 5);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);

    // Initialize the embedded Python interpreter
    pimpl_ = new Impl();
}

Application::~Application() {
    delete pimpl_;
}

} // namespace microbotica::app

#pragma GCC diagnostic pop
