#include "runs/visuals.h"

#include <cctype>
#include <filesystem>
#include <regex>
#include <sstream>

namespace microbotica::runs {

namespace {

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

bool startsWith(const std::string& text, const char* prefix) {
    return text.rfind(prefix, 0) == 0;
}

} // namespace

std::string shellQuoted(const std::string& text) {
    // The characters a POSIX shell takes literally anywhere in a word ('~' and '=' only mean something at its start or
    // in an assignment, so they are quoted too).
    auto literal = [](unsigned char c) {
        return std::isalnum(c) != 0 || c == '/' || c == '.' || c == '_' || c == '-' || c == '+' || c == ':' ||
               c == ',' || c == '@' || c == '%';
    };
    bool plain = !text.empty();
    for (unsigned char c : text) plain = plain && literal(c);
    if (plain) return text;
    std::string quoted = "'";
    for (char c : text) {
        if (c == '\'') {
            quoted += "'\\''";
        } else {
            quoted += c;
        }
    }
    return quoted + "'";
}

std::string visualsFetchCommand(const std::string& folder) {
    return "mime visuals fetch " + shellQuoted(folder);
}

std::string folderOfStage(const std::string& stagePath) {
    namespace fs = std::filesystem;
    if (stagePath.empty()) return {};
    const fs::path stage(stagePath);
    std::error_code ec;
    for (const fs::path& folder : {stage.parent_path().parent_path(), stage.parent_path()}) {
        if (folder.empty()) continue;
        if (fs::is_regular_file(folder / "mime.lock", ec) || fs::is_regular_file(folder / "experiment" / "mime.lock", ec)) {
            return folder.string();
        }
    }
    return {};
}

VisualsView parseVisualsView(const std::string& output) {
    static const std::regex drawnLine(R"(^(\d+) meshes drawn from the asset cache(?: \((.+)\))?\s*$)");
    static const std::regex missingLine(R"(^(\d+) meshes of (\S+) are not in the asset cache\b.*)");
    VisualsView view;
    std::vector<std::string> lines = splitLines(output);
    while (!lines.empty() && lines.back().find_first_not_of(" \t") == std::string::npos) lines.pop_back();
    if (lines.empty()) return view;
    view.layer = lines.back();
    lines.pop_back();
    for (const std::string& line : lines) {
        std::smatch m;
        if (std::regex_match(line, m, drawnLine)) {
            view.drawn = std::stol(m[1].str());
            view.overlay = m[2].matched ? m[2].str() : std::string();
        } else if (std::regex_match(line, m, missingLine)) {
            view.missing[m[2].str()] = std::stol(m[1].str());
        } else {
            view.notice.push_back(line);
        }
    }
    view.lines = std::move(lines);
    return view;
}

std::vector<std::string> noticeCredit(const std::vector<std::string>& notices) {
    std::vector<std::string> lines;
    for (const std::string& notice : notices) {
        for (std::string& line : splitLines(notice)) lines.push_back(std::move(line));
    }
    std::vector<std::string> credit;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const bool next = i + 1 < lines.size() && startsWith(lines[i + 1], "credit:");
        if (startsWith(lines[i], "credit:") || startsWith(lines[i], "licence:") || next) {
            credit.push_back(lines[i]);
        }
    }
    return credit.empty() ? lines : credit;
}

} // namespace microbotica::runs
