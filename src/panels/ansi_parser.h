#pragma once

#include <string>
#include <QString>

namespace microbotica::panels {

/// Converts ANSI escape codes in text to HTML span tags.
///
/// Supports SGR codes: 0 (reset), 1 (bold), 4 (underline),
/// 30-37 (foreground), 39 (default fg), 40-47 (background), 49 (default bg).
/// Stateful — call repeatedly with sequential chunks from QProcess output.
class AnsiParser {
public:
    /// Convert a text chunk with possible ANSI codes to HTML.
    /// Maintains state (bold, color) across calls.
    QString toHtml(const std::string& text);

    /// Reset all formatting state.
    void reset();

private:
    bool bold_ = false;
    bool underline_ = false;
    int fg_color_ = -1;  // -1 = default
    int bg_color_ = -1;

    // Partial escape sequence buffer
    std::string escape_buf_;
    bool in_escape_ = false;

    void applyCode(int code);
    QString colorForIndex(int index, bool foreground) const;
    QString openSpan() const;
};

} // namespace microbotica::panels
