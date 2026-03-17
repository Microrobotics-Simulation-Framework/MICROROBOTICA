#pragma once

#include <QApplication>
#include <QSurfaceFormat>

#include "core/stability.h"

namespace microbotica::app {

/// Custom QApplication subclass that owns the Python interpreter.
///
/// The py::scoped_interpreter must outlive all pybind11 usage.
/// Application is constructed before any scripting or pybind11 modules
/// and destroyed after everything else.
///
/// Uses opaque PIMPL to avoid pybind11 visibility issues in MOC-generated code.
class Application : public QApplication {
    Q_OBJECT

public:
    Application(int& argc, char** argv);
    ~Application() override;

private:
    struct Impl;
    Impl* pimpl_ = nullptr;
};

} // namespace microbotica::app
