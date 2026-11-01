add_executable(microbotica_tests
    main.cpp
    test_core_types.cpp
    test_stub_physics_process.cpp
    test_stub_render_session.cpp
    test_integration_wiring.cpp
    test_scripting_engine.cpp
    test_mime_adapter.cpp
    test_binary_decoder.cpp
    test_cloud_orchestration.cpp
    test_mime_stub.cpp
    test_ansi_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/stubs/stub_physics_process.cpp
    ${CMAKE_SOURCE_DIR}/src/connection/connection_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/connection/ssh_tunnel.cpp
    ${CMAKE_SOURCE_DIR}/src/mime_adapter/mime_physics_process.cpp
    ${CMAKE_SOURCE_DIR}/src/mime_adapter/binary_frame_decoder.cpp
    ${CMAKE_SOURCE_DIR}/src/mime_adapter/mime_stub_physics_process.cpp
    ${CMAKE_SOURCE_DIR}/src/experiment/experiment_runner.cpp
    ${CMAKE_SOURCE_DIR}/src/experiment/cloud_job_launcher.cpp
    ${CMAKE_SOURCE_DIR}/src/experiment/connection_dialog.cpp
    ${CMAKE_SOURCE_DIR}/src/panels/ansi_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/scene/scene_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/scene/results_applicator.cpp
    ${CMAKE_SOURCE_DIR}/src/simulation/simulation_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/scripting/scripting_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/scripting/microbota_module.cpp
    verification/test_layer_stack.cpp
    verification/test_results_applicator.cpp
    verification/test_stub_regression.cpp
    verification/test_scripting_api.cpp
    verification/test_zmq_protocol.cpp
    verification/test_e2e_mime_stub.cpp
)

target_include_directories(microbotica_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
)

target_link_libraries(microbotica_tests PRIVATE
    microbotica_core
    Catch2::Catch2WithMain
    Qt6::Core
    Qt6::Widgets
    pybind11::embed
)

# Pass source dir so tests can find fixture files with absolute paths
target_compile_definitions(microbotica_tests PRIVATE
    MICROBOTICA_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_enable_warnings(microbotica_tests)

if(OpenUSD_FOUND)
    target_include_directories(microbotica_tests SYSTEM PRIVATE ${OpenUSD_INCLUDE_DIRS})
    target_link_libraries(microbotica_tests PRIVATE ${OpenUSD_LIBRARIES})
    target_compile_definitions(microbotica_tests PRIVATE MICROBOTICA_HAS_USD=1)
endif()

if(ZMQ_FOUND)
    target_link_libraries(microbotica_tests PRIVATE ${ZMQ_TARGET})
    target_compile_definitions(microbotica_tests PRIVATE MICROBOTICA_HAS_ZMQ=1)
endif()

if(ZSTD_FOUND)
    target_link_libraries(microbotica_tests PRIVATE ${ZSTD_TARGET})
    if(ZSTD_INCLUDE_DIRS)
        target_include_directories(microbotica_tests SYSTEM PRIVATE ${ZSTD_INCLUDE_DIRS})
    endif()
endif()

include(CTest)
include(Catch)
# DISCOVERY_MODE PRE_TEST defers test enumeration to `ctest` time. The
# default POST_BUILD runs the binary right after linking, which breaks
# ASan/TSan builds before LSAN_OPTIONS can be set. PROPERTIES ENVIRONMENT
# threads the leak-suppression file through to every test invocation so
# ASan runs are clean out of the box.
catch_discover_tests(microbotica_tests
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DISCOVERY_MODE PRE_TEST
    PROPERTIES
        ENVIRONMENT
        "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/lsan.suppressions"
)

# --- Screenshot tests: Storm on Mesa (llvmpipe), headless over EGL --------------------------------------------------
# A separate binary: it links EGL and OpenGL and draws with Mesa, which the sanitizer job need not carry. Built only
# with OpenUSD and an EGL library (the base image has both).
if(OpenUSD_FOUND)
    find_package(OpenGL COMPONENTS OpenGL EGL)
endif()
if(OpenUSD_FOUND AND OpenGL_OpenGL_FOUND AND OpenGL_EGL_FOUND)
    add_executable(microbotica_screenshot_tests
        screenshot/test_storm_screenshots.cpp
        ${CMAKE_SOURCE_DIR}/src/viewport/storm_renderer.cpp
        ${CMAKE_SOURCE_DIR}/src/scene/scene_manager.cpp
        ${CMAKE_SOURCE_DIR}/src/scene/results_applicator.cpp
    )
    target_include_directories(microbotica_screenshot_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_include_directories(microbotica_screenshot_tests SYSTEM PRIVATE ${OpenUSD_INCLUDE_DIRS})
    target_link_libraries(microbotica_screenshot_tests PRIVATE
        microbotica_core
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Gui
        ${OpenUSD_LIBRARIES}
        OpenGL::OpenGL
        OpenGL::EGL
    )
    target_compile_definitions(microbotica_screenshot_tests PRIVATE
        MICROBOTICA_HAS_USD=1
        MICROBOTICA_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        MICROBOTICA_BINARY_DIR="${CMAKE_BINARY_DIR}"
    )
    target_enable_warnings(microbotica_screenshot_tests)
    catch_discover_tests(microbotica_screenshot_tests
        TEST_PREFIX "screenshot: "
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DISCOVERY_MODE PRE_TEST
    )
else()
    message(STATUS "Screenshot tests not built (they need OpenUSD and an EGL library)")
endif()
