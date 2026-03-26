#include "panels/ansi_parser.h"
#include <sstream>

namespace microbotica::panels {

static const char* kAnsiColors[] = {
    "#000000", // 0 black
    "#cc0000", // 1 red
    "#00cc00", // 2 green
    "#cccc00", // 3 yellow
    "#0000cc", // 4 blue
    "#cc00cc", // 5 magenta
    "#00cccc", // 6 cyan
    "#cccccc", // 7 white
};

QString AnsiParser::toHtml(const std::string& text) {
    QString result;
    result.reserve(static_cast<int>(text.size()) * 2);

    // Start with the current formatting state (persisted from prior calls)
    result += openSpan();

    for (char ch : text) {
        if (in_escape_) {
            escape_buf_ += ch;
            if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {
                // End of escape sequence
                in_escape_ = false;
                if (ch == 'm') {
                    // SGR sequence — parse codes
                    // escape_buf_ is like "[31;1m"
                    std::string codes_str = escape_buf_.substr(1, escape_buf_.size() - 2);
                    std::istringstream ss(codes_str);
                    std::string token;
                    while (std::getline(ss, token, ';')) {
                        if (!token.empty()) {
                            try {
                                applyCode(std::stoi(token));
                            } catch (...) {}
                        } else {
                            applyCode(0);
                        }
                    }
                    result += openSpan();
                }
                escape_buf_.clear();
            }
            continue;
        }

        if (ch == '\033') {
            in_escape_ = true;
            escape_buf_.clear();
            result += "</span>";
            continue;
        }

        if (ch == '\n') {
            result += "<br>";
        } else if (ch == '<') {
            result += "&lt;";
        } else if (ch == '>') {
            result += "&gt;";
        } else if (ch == '&') {
            result += "&amp;";
        } else {
            result += QChar(ch);
        }
    }

    return result;
}

void AnsiParser::reset() {
    bold_ = false;
    underline_ = false;
    fg_color_ = -1;
    bg_color_ = -1;
    escape_buf_.clear();
    in_escape_ = false;
}

void AnsiParser::applyCode(int code) {
    if (code == 0) { reset(); return; }
    if (code == 1) { bold_ = true; return; }
    if (code == 4) { underline_ = true; return; }
    if (code >= 30 && code <= 37) { fg_color_ = code - 30; return; }
    if (code == 39) { fg_color_ = -1; return; }
    if (code >= 40 && code <= 47) { bg_color_ = code - 40; return; }
    if (code == 49) { bg_color_ = -1; return; }
}

QString AnsiParser::colorForIndex(int index, bool /*foreground*/) const {
    if (index >= 0 && index < 8) {
        return QString::fromUtf8(kAnsiColors[index]);
    }
    return {};
}

QString AnsiParser::openSpan() const {
    QString style;
    if (fg_color_ >= 0) {
        style += "color:" + colorForIndex(fg_color_, true) + ";";
    }
    if (bg_color_ >= 0) {
        style += "background-color:" + colorForIndex(bg_color_, false) + ";";
    }
    if (bold_) {
        style += "font-weight:bold;";
    }
    if (underline_) {
        style += "text-decoration:underline;";
    }
    if (style.isEmpty()) {
        return "<span>";
    }
    return "<span style=\"" + style + "\">";
}

} // namespace microbotica::panels
