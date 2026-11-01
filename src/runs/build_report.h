#pragma once

// MIME's build report, summarised for a client (format `mime.report/1`; MIME's docs/live.md, "The build report's
// summary"): what a built experiment claims, as `hello.report` and a run folder's `report.json` give it. Its validity
// lines (each declared, computed at the build, or a capability record's limit), what each run is watched against, every
// flag a run may raise and what it means, each setting's role and default, and the profile's stamp.
//
// A run folder written before MIME 0.3's phase 7n-b has no report.json; one of a newer format is not read, and says so
// (as MIME's own `RunResult.read` does: `report` None, `report_unreadable` the format the file states).
//
// Free of Qt and USD: nlohmann_json and the standard library only.

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace microbotica::runs {

/// The format this client reads.
inline constexpr const char* kReportFormat = "mime.report/1";

/// One validity line and how MIME holds it.
struct ValidityLine {
    std::string owner;  ///< the node or entry it is about
    std::string kind;   ///< "declared" (not yet checked), "computed" (at the build) or "record" (a capability record's)
    std::string line;

    /// Declared and not yet checked: to be shown as such, never as a verdict.
    bool declaredOnly() const { return kind == "declared"; }
};

/// A flag a run may raise.
struct ReportFlag {
    std::string path;     ///< as frames and outputs.json name it, e.g. "drag:umr.tilt_fired"
    std::string owner;
    std::string entry;
    std::string node;
    std::string meaning;  ///< what it says when raised
    nlohmann::json limits;  ///< [low, high] where it is read against limits (null for an unbounded side), or null
};

/// A setting's declared role and the default the build took.
struct ReportSetting {
    std::string path;
    std::string role;          ///< design, physical, numerical, standard; empty when its entry declares none
    std::string defaultKind;   ///< determined, convention, numerical, none, undeclared
    std::optional<std::string> taken;  ///< the default the build took, as text
    std::string source;
    std::vector<std::string> pending;
    std::vector<std::string> listed;
};

/// The experiment's profile and what its stamp says.
struct ReportProfile {
    std::string profile;   ///< exploratory, research, strict
    std::string stamp;     ///< the stamp's text
    std::vector<std::string> pending;
    std::vector<std::string> listed;
    std::vector<std::string> measured;
    std::vector<std::string> defaults;  ///< each default the build took, as text
};

/// The report.
struct BuildReport {
    std::string experiment;
    std::optional<ReportProfile> profile;
    std::vector<ValidityLine> validity;
    std::vector<std::string> watches;
    std::vector<ReportFlag> flags;
    std::vector<ReportSetting> settings;
    std::vector<std::string> summary;  ///< the report's text lines
    nlohmann::json raw;
};

/// What reading a report gave: the report, or why there is none. Both empty: there was no report (a run folder
/// written before reports reached them, a hello of an older MIME).
struct ReportRead {
    std::optional<BuildReport> report;
    /// Why a report there was not read: the format it states (a newer one), "not JSON" or "not a JSON object".
    std::optional<std::string> unreadable;
};

/// A report as JSON (hello's `report`; null or absent: none).
ReportRead parseReport(const nlohmann::json& report);

/// A report file (a run folder's report.json); a file that does not exist: none.
ReportRead readReportFile(const std::string& path);

} // namespace microbotica::runs
