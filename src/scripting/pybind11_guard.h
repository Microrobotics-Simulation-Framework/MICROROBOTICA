#pragma once

// Qt defines "slots" as a macro which conflicts with pybind11.
// This guard pushes the macro, undefines it for pybind11 includes,
// then restores it.

#pragma push_macro("slots")
#undef slots
#include <pybind11/pybind11.h>
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#pragma pop_macro("slots")

namespace py = pybind11;
