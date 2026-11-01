// MIME's coded errors in their shape (MIME 0.3, docs/errors.md): read from a MIME process's stderr and from a
// live-protocol reply, and shown by the MIME console. The texts below are MIME 0.3's own output, captured from
// `mime serve` and a script (the traceback file paths shortened).

#include <catch2/catch_test_macros.hpp>

#include "core/mime_error.h"
#include "core/verification_registry.h"
#include "panels/mime_error_html.h"

#include <sstream>
#include <string>
#include <vector>

using microbotica::core::MimeErrorReport;
using microbotica::core::MimeErrorTextReader;
using microbotica::core::MimeRefusal;
using microbotica::core::refusalsFromJson;

namespace {

struct Read {
    std::vector<std::string> plain;
    std::vector<MimeErrorReport> reports;
};

Read readAll(const std::string& text) {
    MimeErrorTextReader reader;
    Read out;
    auto take = [&out](const MimeErrorTextReader::Fed& fed) {
        out.plain.insert(out.plain.end(), fed.plain.begin(), fed.plain.end());
        out.reports.insert(out.reports.end(), fed.reports.begin(), fed.reports.end());
    };
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        take(reader.feed(line));
    }
    take(reader.finish());
    return out;
}

// `mime serve <a folder that is not there>`, its whole stderr (exit code 2).
const char* kServeRefused =
    "mime serve is refused:\n"
    "[user] load.missing-folder: there is no folder '/data/a0'\n"
    "    do: name the folder Experiment.save wrote, as in `mime run <folder> ...`\n"
    "    see: https://microrobotica.org/mime/0.3.0/pages/errors/load.missing-folder\n"
    "The full traceback is in /home/u/.cache/mime/tracebacks/mime-1-load.missing-folder.txt; --traceback shows it "
    "here.\n";

// A script's run refused, as Python's excepthook prints it through MIME's short form.
const char* kScriptRefused =
    "Traceback (most recent call last; MIME's, JAX's and MADDENING's frames left out):\n"
    "  File \"make.py\", line 17, in <module>\n"
    "    w = a0().build().run_to(d, s(2.0), stage_every=ms(16.5))\n"
    "ArgumentRefused: the call is refused:\n"
    "[user] run.chunks: a run of 4000 base steps (2 s) is not a whole number of chunks of 33 steps (0.0165 s)\n"
    "    why: a hook is called between compiled chunks of chunk= steps, so a run with one is whole chunks\n"
    "    do: run a whole number of chunks: run(s(...)) of 1.9965 s or 2.013 s\n"
    "    see: https://microrobotica.org/mime/0.3.0/pages/errors/run.chunks\n"
    "The full traceback is in /home/u/.cache/mime/tracebacks/mime-2-run.chunks.txt; MIME_TRACEBACK=full shows it "
    "here.\n";

} // namespace

REGISTER_VERIFICATION_BENCHMARK(
    MBCA_VER_013,
    "MBCA-VER-013",
    "MBCA-COMP-042",
    microbotica::core::BenchmarkType::DataIntegrity,
    "MIME's coded errors keep every part (class, code, message, why, do, at, see) from stderr and replies",
    __FILE__,
    "MIME error text: mime serve's refusal is read whole, every part in place"
)

TEST_CASE("MIME error text: mime serve's refusal is read whole, every part in place", "[unit][mime-error]") {
    const Read read = readAll(kServeRefused);
    REQUIRE(read.plain.empty());
    REQUIRE(read.reports.size() == 1);
    const MimeErrorReport& report = read.reports[0];
    CHECK(report.headline == "mime serve is refused");
    REQUIRE(report.refusals.size() == 1);
    const MimeRefusal& r = report.refusals[0];
    CHECK(r.cls == "user");
    CHECK(r.code == "load.missing-folder");
    CHECK(r.message == "there is no folder '/data/a0'");
    CHECK(r.why.empty());
    CHECK(r.action == "name the folder Experiment.save wrote, as in `mime run <folder> ...`");
    CHECK(r.where.empty());
    CHECK(r.page == "https://microrobotica.org/mime/0.3.0/pages/errors/load.missing-folder");
    CHECK(report.tracebackNote.rfind("The full traceback is in /home/u/.cache/mime/tracebacks/", 0) == 0);
}

TEST_CASE("MIME error text: a script's short form keeps its frames as plain lines and its error in shape",
          "[unit][mime-error]") {
    const Read read = readAll(kScriptRefused);
    REQUIRE(read.plain.size() == 3);
    CHECK(read.plain[0].rfind("Traceback (most recent call last", 0) == 0);
    CHECK(read.plain[1] == "  File \"make.py\", line 17, in <module>");
    REQUIRE(read.reports.size() == 1);
    CHECK(read.reports[0].headline == "ArgumentRefused: the call is refused");
    const MimeRefusal& r = read.reports[0].refusals.at(0);
    CHECK(r.code == "run.chunks");
    CHECK(r.why == "a hook is called between compiled chunks of chunk= steps, so a run with one is whole chunks");
    CHECK(r.action == "run a whole number of chunks: run(s(...)) of 1.9965 s or 2.013 s");
}

TEST_CASE("MIME error text: several reasons, an `at` line and a defect's footer", "[unit][mime-error]") {
    const Read read = readAll(
        "BuildRefused: the build is refused for 2 reasons:\n"
        "[user] setting.value: interaction drag:ball (drag 0.3.0): spacing is None m, not a length above zero\n"
        "    why: the mrs rung samples the body's surface with neighbouring samples about spacing apart\n"
        "    do: write spacing=mm(...), where interaction drag:ball is declared (Drag(...))\n"
        "    at: settle.py, line 12\n"
        "    see: https://microrobotica.org/mime/0.3.0/pages/errors/setting.value\n"
        "[internal] build.plan: a plan MIME made is inconsistent\n"
        "    do: report it with the file named below\n"
        "    see: https://microrobotica.org/mime/0.3.0/pages/errors/build.plan\n"
        "This is a defect in MIME: report it at https://example.org/issues, with the file named below.\n"
        "The full traceback is in /tmp/t.txt; MIME_TRACEBACK=full shows it here.\n"
        "after the error\n");
    REQUIRE(read.reports.size() == 1);
    const MimeErrorReport& report = read.reports[0];
    CHECK(report.headline == "BuildRefused: the build is refused for 2 reasons");
    REQUIRE(report.refusals.size() == 2);
    CHECK(report.refusals[0].where == "settle.py, line 12");
    CHECK(report.refusals[1].cls == "internal");
    CHECK(report.refusals[1].code == "build.plan");
    CHECK(report.tracebackNote.find("This is a defect in MIME") == 0);
    REQUIRE(read.plain == std::vector<std::string>{"after the error"});
}

TEST_CASE("MIME error text: ordinary output, a lone headline-like line and ANSI colour stay as they are",
          "[unit][mime-error]") {
    const Read plain = readAll("step 10\nthe download failed:\nretrying\n");
    CHECK(plain.reports.empty());
    CHECK(plain.plain == std::vector<std::string>{"step 10", "the download failed:", "retrying"});

    const Read coloured = readAll(
        "\x1b[1mmime serve is refused:\x1b[0m\n"
        "\x1b[33m[environment] env.missing: JAX is not installed\x1b[0m\n"
        "    do: pip install jax\n");
    REQUIRE(coloured.reports.size() == 1);
    CHECK(coloured.reports[0].headline == "mime serve is refused");
    CHECK(coloured.reports[0].refusals.at(0).cls == "environment");
    CHECK(coloured.reports[0].refusals.at(0).message == "JAX is not installed");
    CHECK(coloured.reports[0].refusals.at(0).action == "pip install jax");
}

TEST_CASE("MIME error JSON: a refusing reply's errors keep every part", "[unit][mime-error]") {
    const auto reply = nlohmann::json::parse(R"({
        "protocol": "mime.live/1", "ok": false, "command": "set",
        "errors": [{"code": "live.input", "class": "user", "message": "no live input named 'gain'",
                    "why": "a controller declares the inputs a session sets",
                    "action": "set one of: offset", "where": null,
                    "page": "https://microrobotica.org/mime/0.3.0/pages/errors/live.input"},
                   "not an object"]})");
    const auto refusals = refusalsFromJson(reply["errors"]);
    REQUIRE(refusals.size() == 1);
    CHECK(refusals[0].code == "live.input");
    CHECK(refusals[0].cls == "user");
    CHECK(refusals[0].message == "no live input named 'gain'");
    CHECK(refusals[0].action == "set one of: offset");
    CHECK(refusals[0].where.empty());
    CHECK(refusalsFromJson(nlohmann::json::object()).empty());
}

TEST_CASE("MIME console HTML: the code, what to do and the page as a link, every text escaped", "[unit][mime-error]") {
    MimeErrorReport report;
    report.headline = "mime serve is refused";
    MimeRefusal r;
    r.cls = "user";
    r.code = "load.missing-folder";
    r.message = "there is no folder '<a0>'";
    r.action = "name the folder Experiment.save wrote, as in `mime run <folder> ...`";
    r.page = "https://microrobotica.org/mime/0.3.0/pages/errors/load.missing-folder";
    report.refusals.push_back(r);
    const QString html = microbotica::panels::mimeErrorReportHtml(report);
    CHECK(html.contains(QStringLiteral("<b>load.missing-folder</b>")));
    CHECK(html.contains(QStringLiteral("&lt;a0&gt;")));
    CHECK_FALSE(html.contains(QStringLiteral("<a0>")));
    CHECK(html.contains(QStringLiteral("what to do:")));
    CHECK(html.contains(QStringLiteral(
        "<a href=\"https://microrobotica.org/mime/0.3.0/pages/errors/load.missing-folder\">")));
}
