add_executable(microbotica_tests
    main.cpp
    test_core_types.cpp
    test_stub_physics_process.cpp
    test_stub_render_session.cpp
    test_integration_wiring.cpp
    test_scripting_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/stubs/stub_physics_process.cpp
    ${CMAKE_SOURCE_DIR}/src/scene/scene_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/scene/results_applicator.cpp
    ${CMAKE_SOURCE_DIR}/src/simulation/simulation_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/scripting/scripting_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/scripting/microbota_module.cpp
    verification/test_layer_stack.cpp
    verification/test_results_applicator.cpp
    verification/test_stub_regression.cpp
    verification/test_scripting_api.cpp
)

target_include_directories(microbotica_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(microbotica_tests PRIVATE
    microbotica_core
    Catch2::Catch2WithMain
    Qt6::Core
    Qt6::Widgets
    pybind11::embed
)

target_enable_warnings(microbotica_tests)

if(OpenUSD_FOUND)
    target_include_directories(microbotica_tests SYSTEM PRIVATE ${OpenUSD_INCLUDE_DIRS})
    target_link_libraries(microbotica_tests PRIVATE ${OpenUSD_LIBRARIES})
    target_compile_definitions(microbotica_tests PRIVATE MICROBOTICA_HAS_USD=1)
endif()

include(CTest)
include(Catch)
catch_discover_tests(microbotica_tests)
