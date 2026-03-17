cmake_minimum_required(VERSION 3.25)
project(MICROBOTICA VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Options ---
option(BUILD_PYTHON_MODULE "Build standalone Python module" OFF)
option(BUILD_TESTS "Build test suite" ON)

# --- Compiler warnings module ---
include(cmake/CompilerWarnings.cmake)

# --- Find dependencies ---

# nlohmann_json (required for core)
find_package(nlohmann_json 3.11.0 REQUIRED)

# spdlog (required for core)
find_package(spdlog REQUIRED)

# Catch2 (required for tests)
if(BUILD_TESTS)
    find_package(Catch2 3 REQUIRED)
endif()

# Qt6 (required for application)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGLWidgets)
qt_standard_project_setup()

# pybind11 (required for scripting)
find_package(pybind11 REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

# ZeroMQ (optional for now — Phase 1 IPC)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(ZMQ QUIET libzmq)
endif()

# OpenUSD (optional — scene management)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(OpenUSD QUIET)
if(OpenUSD_FOUND)
    message(STATUS "OpenUSD found: ${OpenUSD_INCLUDE_DIRS}")
    add_compile_definitions(MICROBOTICA_HAS_USD=1)
else()
    message(STATUS "OpenUSD not found — USD-dependent features will be stubbed")
endif()

# --- microbotica_core library (header-only + verification_registry.cpp) ---
# Zero Qt/USD/Python dependencies. Only stdlib + nlohmann_json + spdlog.
add_library(microbotica_core STATIC)
target_sources(microbotica_core PRIVATE
    src/core/verification_registry.cpp
)
target_include_directories(microbotica_core PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(microbotica_core PUBLIC
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)
target_enable_warnings(microbotica_core)

# --- microbotica application ---
add_executable(microbotica)
target_sources(microbotica PRIVATE
    src/main.cpp
    src/stubs/stub_physics_process.cpp
    src/scene/scene_manager.cpp
    src/scene/results_applicator.cpp
    src/scene/prim_selection.cpp
    src/simulation/simulation_controller.cpp
    src/scripting/scripting_engine.cpp
    src/scripting/microbota_module.cpp
    src/viewport/viewport_widget.cpp
    src/viewport/local_viewport.cpp
    src/viewport/software_viewport.cpp
    src/viewport/camera_controller.cpp
    src/panels/scene_hierarchy_panel.cpp
    src/panels/property_panel.cpp
    src/panels/timeline_panel.cpp
    src/panels/console_widget.cpp
    src/app/application.cpp
    src/app/main_window.cpp
)
target_include_directories(microbotica PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(microbotica PRIVATE
    microbotica_core
    Qt6::Core
    Qt6::Widgets
    Qt6::OpenGLWidgets
    pybind11::embed
)
if(OpenUSD_FOUND)
    target_include_directories(microbotica PRIVATE ${OpenUSD_INCLUDE_DIRS})
    target_link_libraries(microbotica PRIVATE ${OpenUSD_LIBRARIES})
endif()
target_enable_warnings(microbotica)

# --- Python module (standalone) ---
if(BUILD_PYTHON_MODULE)
    pybind11_add_module(microrobotica_py)
    target_include_directories(microrobotica_py PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(microrobotica_py PRIVATE microbotica_core)
    if(OpenUSD_FOUND)
        target_include_directories(microrobotica_py PRIVATE ${OpenUSD_INCLUDE_DIRS})
        target_link_libraries(microrobotica_py PRIVATE ${OpenUSD_LIBRARIES})
    endif()
endif()

# --- Tests ---
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
