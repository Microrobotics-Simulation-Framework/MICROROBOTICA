// pybind11_guard.h must come before any Qt headers to handle "slots" macro
#include "scripting/pybind11_guard.h"

#include <catch2/catch_test_macros.hpp>
#include "scripting/scripting_engine.h"

using namespace microbotica::scripting;

TEST_CASE("ScriptingEngine: initialize", "[unit][scripting]") {
    py::scoped_interpreter guard{};

    ScriptingEngine engine;
    REQUIRE(engine.initialize());
    REQUIRE(engine.isInitialized());
}

TEST_CASE("ScriptingEngine: execute simple code", "[unit][scripting]") {
    py::scoped_interpreter guard{};

    ScriptingEngine engine;
    engine.initialize();

    std::string out, err;
    bool ok = engine.execute("print('hello')", out, err);
    REQUIRE(ok);
    REQUIRE(out == "hello\n");
    REQUIRE(err.empty());
}

TEST_CASE("ScriptingEngine: import microrobotica", "[unit][scripting]") {
    py::scoped_interpreter guard{};

    ScriptingEngine engine;
    engine.initialize();

    std::string out, err;
    bool ok = engine.execute("import microrobotica", out, err);
    REQUIRE(ok);
}

TEST_CASE("ScriptingEngine: sim().current_time() returns 0.0", "[unit][scripting]") {
    py::scoped_interpreter guard{};

    ScriptingEngine engine;
    engine.initialize();

    std::string out, err;
    bool ok = engine.execute(
        "import microrobotica\n"
        "t = microrobotica.sim().current_time()\n"
        "print(t)",
        out, err);
    REQUIRE(ok);
    REQUIRE(out == "0.0\n");
}

TEST_CASE("ScriptingEngine: sim().status() returns idle", "[unit][scripting]") {
    py::scoped_interpreter guard{};

    ScriptingEngine engine;
    engine.initialize();

    std::string out, err;
    bool ok = engine.execute(
        "import microrobotica\n"
        "print(microrobotica.sim().status())",
        out, err);
    REQUIRE(ok);
    REQUIRE(out == "idle\n");
}

TEST_CASE("ScriptingEngine: syntax error reports failure", "[unit][scripting]") {
    py::scoped_interpreter guard{};

    ScriptingEngine engine;
    engine.initialize();

    std::string out, err;
    bool ok = engine.execute("def foo(", out, err);
    REQUIRE_FALSE(ok);
    REQUIRE_FALSE(err.empty());
}

TEST_CASE("ScriptingEngine: send_parameters with non-string key raises TypeError",
          "[unit][scripting]") {
    py::scoped_interpreter guard{};

    ScriptingEngine engine;
    engine.initialize();

    std::string out, err;
    bool ok = engine.execute(
        "import microrobotica\n"
        "try:\n"
        "    microrobotica.sim().send_parameters({123: 'value'})\n"
        "    print('no_error')\n"
        "except TypeError as e:\n"
        "    print('type_error')\n",
        out, err);
    REQUIRE(ok);
    REQUIRE(out == "type_error\n");
}
