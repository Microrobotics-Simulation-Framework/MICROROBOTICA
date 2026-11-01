#pragma once

// MIME's coded errors in their shape (MIME 0.3, docs/errors.md), read from the two places MICROROBOTICA meets them:
//
// - **Text**, on a MIME process's stderr (`mime serve` refusing to start, a run failing):
//
//       mime serve is refused:
//       [user] load.missing-folder: there is no folder '/data/a0'
//           why: <one plain sentence, where the message does not say it>
//           do: name the folder Experiment.save wrote, as in `mime run <folder> ...`
//           at: <the script's file and line, where MIME knows one>
//           see: https://microrobotica.org/mime/0.3.0/pages/errors/load.missing-folder
//       The full traceback is in <file>; --traceback shows it here.
//
// - **JSON**, in a refusing reply of the live protocol: `errors: [{code, class, message, why, action, where, page}]`.
//
// Header-only and free of Qt, USD and Python, as src/core/ is.

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace microbotica::core {

/// One reason MIME refused something: its class (who can act on it), its code (which names its page), what happened,
/// why, what to do, where, and the code's page.
struct MimeRefusal {
    std::string cls;      ///< "user", "environment", "internal" or "unrecognised"
    std::string code;     ///< e.g. "load.missing-folder"
    std::string message;  ///< what happened, in the experiment's own names
    std::string why;      ///< one plain sentence, or empty
    std::string action;   ///< what to do ("do:")
    std::string where;    ///< the script's file and line ("at:"), or empty
    std::string page;     ///< the code's documentation page ("see:"), a URL for MIME's own codes

    bool operator==(const MimeRefusal& other) const {
        return cls == other.cls && code == other.code && message == other.message && why == other.why
            && action == other.action && where == other.where && page == other.page;
    }
};

/// What MIME refused (its headline, "mime serve is refused"), every reason, and where the full traceback was written.
struct MimeErrorReport {
    std::string headline;
    std::vector<MimeRefusal> refusals;
    std::string tracebackNote;  ///< "The full traceback is in ..." (empty when not printed)
};

namespace mime_error_detail {

inline std::string stripAnsi(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\x1b' && i + 1 < text.size() && text[i + 1] == '[') {
            i += 2;
            while (i < text.size() && !(text[i] >= '@' && text[i] <= '~')) {
                ++i;
            }
            continue;
        }
        out.push_back(text[i]);
    }
    return out;
}

inline bool isCodeChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-'
        || c == '_' || c == ':' || c == '/';
}

/// "[user] load.missing-folder: there is no folder ..." -> a refusal with class, code and message.
inline std::optional<MimeRefusal> refusalHead(const std::string& line) {
    if (line.size() < 4 || line[0] != '[') {
        return std::nullopt;
    }
    const std::size_t close = line.find("] ");
    if (close == std::string::npos || close < 2) {
        return std::nullopt;
    }
    const std::string cls = line.substr(1, close - 1);
    for (char c : cls) {
        if (!(c >= 'a' && c <= 'z')) {
            return std::nullopt;
        }
    }
    const std::size_t codeStart = close + 2;
    const std::size_t colon = line.find(": ", codeStart);
    if (colon == std::string::npos || colon == codeStart) {
        return std::nullopt;
    }
    const std::string code = line.substr(codeStart, colon - codeStart);
    for (char c : code) {
        if (!isCodeChar(c)) {
            return std::nullopt;
        }
    }
    MimeRefusal refusal;
    refusal.cls = cls;
    refusal.code = code;
    refusal.message = line.substr(colon + 2);
    return refusal;
}

/// "    why: ..." and its kin: the part's name and its text, or nothing.
inline std::optional<std::pair<std::string, std::string>> refusalPart(const std::string& line) {
    if (line.rfind("    ", 0) != 0) {
        return std::nullopt;
    }
    for (const char* name : {"why", "do", "at", "see"}) {
        const std::string prefix = std::string("    ") + name + ": ";
        if (line.rfind(prefix, 0) == 0) {
            return std::make_pair(std::string(name), line.substr(prefix.size()));
        }
    }
    return std::nullopt;
}

} // namespace mime_error_detail

/// The refusals of a live-protocol reply's `errors` array. Entries that are not objects are skipped; missing fields
/// are empty.
inline std::vector<MimeRefusal> refusalsFromJson(const nlohmann::json& errors) {
    std::vector<MimeRefusal> out;
    if (!errors.is_array()) {
        return out;
    }
    auto text = [](const nlohmann::json& object, const char* key) {
        const auto it = object.find(key);
        return (it != object.end() && it->is_string()) ? it->get<std::string>() : std::string();
    };
    for (const auto& e : errors) {
        if (!e.is_object()) {
            continue;
        }
        MimeRefusal r;
        r.cls = text(e, "class");
        r.code = text(e, "code");
        r.message = text(e, "message");
        r.why = text(e, "why");
        r.action = text(e, "action");
        r.where = text(e, "where");
        r.page = text(e, "page");
        out.push_back(std::move(r));
    }
    return out;
}

/// Reads MIME's coded errors out of a process's output, line by line. Every line comes back out of feed(), either as
/// a plain line to show as it is or held as part of a report, which feed() returns once the report ends (at the first
/// line that is not one of its parts, or at finish()).
class MimeErrorTextReader {
public:
    /// What one line of output turned out to be.
    struct Fed {
        std::vector<std::string> plain;               ///< lines that are not part of a report, in order
        std::vector<MimeErrorReport> reports;         ///< reports completed by this line
    };

    /// Feed one line (without its newline; ANSI colour codes are ignored).
    Fed feed(const std::string& rawLine) {
        Fed fed;
        const std::string line = mime_error_detail::stripAnsi(rawLine);
        if (auto head = mime_error_detail::refusalHead(line)) {
            if (!inReport_) {
                startReport(fed);
            }
            current_.refusals.push_back(std::move(*head));
            return fed;
        }
        if (inReport_ && !current_.refusals.empty()) {
            if (auto part = mime_error_detail::refusalPart(line)) {
                MimeRefusal& r = current_.refusals.back();
                const std::string& name = part->first;
                if (name == "why") r.why = part->second;
                else if (name == "do") r.action = part->second;
                else if (name == "at") r.where = part->second;
                else if (name == "see") r.page = part->second;
                return fed;
            }
            if (line.rfind("This is a defect in", 0) == 0 || line.rfind("The full traceback", 0) == 0
                || line.rfind("An error MIME raised with no code", 0) == 0) {
                if (!current_.tracebackNote.empty()) {
                    current_.tracebackNote += "\n";
                }
                current_.tracebackNote += line;
                if (line.rfind("The full traceback", 0) == 0) {
                    closeReport(fed);  // the last line MIME prints for an error
                }
                return fed;
            }
            closeReport(fed);
        }
        // Not (or no longer) in a report. A headline ("mime serve is refused:", "BuildRefused: the build is
        // refused for 2 reasons:") is held until the next line says whether a refusal follows it.
        if (pendingHeadline_) {
            fed.plain.push_back(*pendingHeadline_);
            pendingHeadline_.reset();
        }
        if (looksLikeHeadline(line)) {
            pendingHeadline_ = line;
            return fed;
        }
        fed.plain.push_back(rawLine);
        return fed;
    }

    /// The end of the output: a report still open is complete, a held headline is plain.
    Fed finish() {
        Fed fed;
        if (inReport_) {
            closeReport(fed);
        }
        if (pendingHeadline_) {
            fed.plain.push_back(*pendingHeadline_);
            pendingHeadline_.reset();
        }
        return fed;
    }

private:
    static bool looksLikeHeadline(const std::string& line) {
        if (line.empty() || line.back() != ':') {
            return false;
        }
        for (const char* word : {"refused", "failed", "does not recognise"}) {
            if (line.find(word) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    void startReport(Fed& fed) {
        (void)fed;
        inReport_ = true;
        current_ = MimeErrorReport{};
        if (pendingHeadline_) {
            std::string h = *pendingHeadline_;
            pendingHeadline_.reset();
            if (!h.empty() && h.back() == ':') {
                h.pop_back();
            }
            current_.headline = h;
        }
    }

    void closeReport(Fed& fed) {
        fed.reports.push_back(std::move(current_));
        current_ = MimeErrorReport{};
        inReport_ = false;
    }

    bool inReport_ = false;
    MimeErrorReport current_;
    std::optional<std::string> pendingHeadline_;
};

} // namespace microbotica::core
