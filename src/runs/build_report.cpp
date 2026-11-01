#include "runs/build_report.h"

#include <filesystem>
#include <fstream>

namespace microbotica::runs {

namespace {

std::string text(const nlohmann::json& object, const char* key) {
    if (!object.is_object()) return {};
    const auto it = object.find(key);
    return (it != object.end() && it->is_string()) ? it->get<std::string>() : std::string();
}

std::vector<std::string> lines(const nlohmann::json& object, const char* key) {
    std::vector<std::string> out;
    if (!object.is_object()) return out;
    const auto it = object.find(key);
    if (it == object.end() || !it->is_array()) return out;
    for (const auto& item : *it) {
        out.push_back(item.is_string() ? item.get<std::string>() : item.dump());
    }
    return out;
}

} // namespace

ReportRead parseReport(const nlohmann::json& j) {
    ReportRead read;
    if (j.is_null()) {
        return read;
    }
    if (!j.is_object()) {
        read.unreadable = "not a JSON object";
        return read;
    }
    const auto format = j.find("format");
    if (format == j.end() || !format->is_string() || format->get<std::string>() != kReportFormat) {
        read.unreadable = (format != j.end() && format->is_string()) ? format->get<std::string>()
                          : (format == j.end() ? std::string("None") : format->dump());
        return read;
    }

    BuildReport r;
    r.raw = j;
    r.experiment = text(j, "experiment");
    if (j.contains("profile") && j["profile"].is_object()) {
        const auto& p = j["profile"];
        ReportProfile profile;
        profile.profile = text(p, "profile");
        profile.stamp = text(p, "stamp");
        profile.pending = lines(p, "pending");
        profile.listed = lines(p, "listed");
        profile.measured = lines(p, "measured");
        profile.defaults = lines(p, "defaults");
        r.profile = std::move(profile);
    }
    if (j.contains("validity") && j["validity"].is_array()) {
        for (const auto& v : j["validity"]) {
            ValidityLine line{text(v, "owner"), text(v, "kind"), text(v, "line")};
            if (!line.line.empty()) r.validity.push_back(std::move(line));
        }
    }
    r.watches = lines(j, "watches");
    if (j.contains("flags") && j["flags"].is_array()) {
        for (const auto& f : j["flags"]) {
            ReportFlag flag;
            flag.path = text(f, "path");
            flag.owner = text(f, "owner");
            flag.entry = text(f, "entry");
            flag.node = text(f, "node");
            flag.meaning = text(f, "meaning");
            flag.limits = f.is_object() && f.contains("limits") ? f["limits"] : nlohmann::json();
            if (!flag.path.empty()) r.flags.push_back(std::move(flag));
        }
    }
    if (j.contains("settings") && j["settings"].is_array()) {
        for (const auto& s : j["settings"]) {
            ReportSetting setting;
            setting.path = text(s, "path");
            setting.role = text(s, "role");
            setting.defaultKind = text(s, "default");
            if (s.is_object() && s.contains("taken") && s["taken"].is_string()) {
                setting.taken = s["taken"].get<std::string>();
            }
            setting.source = text(s, "source");
            setting.pending = lines(s, "pending");
            setting.listed = lines(s, "listed");
            if (!setting.path.empty()) r.settings.push_back(std::move(setting));
        }
    }
    r.summary = lines(j, "summary");
    read.report = std::move(r);
    return read;
}

ReportRead readReportFile(const std::string& path) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) {
        return {};
    }
    std::ifstream in(path);
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(in);
    } catch (const nlohmann::json::parse_error&) {
        ReportRead read;
        read.unreadable = "not JSON";
        return read;
    }
    if (j.is_null()) {
        ReportRead read;
        read.unreadable = "not a JSON object";
        return read;
    }
    return parseReport(j);
}

} // namespace microbotica::runs
