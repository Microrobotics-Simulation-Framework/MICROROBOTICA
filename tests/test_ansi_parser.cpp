#include <catch2/catch_test_macros.hpp>
#include "panels/ansi_parser.h"

using namespace microbotica::panels;

TEST_CASE("AnsiParser: plain text passes through", "[unit][ansi]") {
    AnsiParser parser;
    QString result = parser.toHtml("hello world");
    REQUIRE(result.contains("hello world"));
}

TEST_CASE("AnsiParser: HTML entities escaped", "[unit][ansi]") {
    AnsiParser parser;
    QString result = parser.toHtml("<script>&amp;</script>");
    REQUIRE(result.contains("&lt;script&gt;"));
    REQUIRE(result.contains("&amp;amp;"));
    REQUIRE_FALSE(result.contains("<script>"));
}

TEST_CASE("AnsiParser: newlines become <br>", "[unit][ansi]") {
    AnsiParser parser;
    QString result = parser.toHtml("line1\nline2");
    REQUIRE(result.contains("<br>"));
}

TEST_CASE("AnsiParser: red foreground", "[unit][ansi]") {
    AnsiParser parser;
    QString result = parser.toHtml("\033[31mred text\033[0m");
    REQUIRE(result.contains("color:#cc0000"));
    REQUIRE(result.contains("red text"));
}

TEST_CASE("AnsiParser: bold", "[unit][ansi]") {
    AnsiParser parser;
    QString result = parser.toHtml("\033[1mbold text\033[0m");
    REQUIRE(result.contains("font-weight:bold"));
}

TEST_CASE("AnsiParser: combined bold + color", "[unit][ansi]") {
    AnsiParser parser;
    QString result = parser.toHtml("\033[1;32mgreen bold\033[0m");
    REQUIRE(result.contains("font-weight:bold"));
    REQUIRE(result.contains("color:#00cc00"));
}

TEST_CASE("AnsiParser: reset clears formatting", "[unit][ansi]") {
    AnsiParser parser;
    // First set red
    parser.toHtml("\033[31m");
    // Then reset
    QString result = parser.toHtml("\033[0mnormal");
    // After reset, the span should not have a color style
    REQUIRE(result.contains("<span>"));
}

TEST_CASE("AnsiParser: state persists across calls", "[unit][ansi]") {
    AnsiParser parser;
    // Set bold in first chunk
    parser.toHtml("\033[1m");
    // Second chunk should inherit bold
    QString result = parser.toHtml("still bold");
    REQUIRE(result.contains("font-weight:bold"));
}

TEST_CASE("AnsiParser: all 8 foreground colors", "[unit][ansi]") {
    AnsiParser parser;
    for (int i = 30; i <= 37; ++i) {
        std::string seq = "\033[" + std::to_string(i) + "mtest\033[0m";
        QString result = parser.toHtml(seq);
        REQUIRE(result.contains("color:#"));
    }
}

TEST_CASE("AnsiParser: background color", "[unit][ansi]") {
    AnsiParser parser;
    QString result = parser.toHtml("\033[41mred bg\033[0m");
    REQUIRE(result.contains("background-color:#cc0000"));
}
