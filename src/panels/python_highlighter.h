#pragma once

#include <QColor>
#include <QRegularExpression>
#include <QString>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <vector>

namespace microbotica::panels {

/// Colour scheme for the Python highlighter.
///
/// Pick a preset by name via PythonHighlighter::themeByName, or build one
/// directly. Persisted in QSettings under `code_highlight/theme` so the
/// Settings dialog can offer a combobox of named themes.
struct PythonHighlightTheme {
    QString name;
    QColor keyword;
    QColor builtin;
    QColor string;
    QColor comment;
    QColor number;
    QColor decorator;
    bool keywordBold = true;
    bool commentItalic = true;
};

/// Python syntax highlighter for QPlainTextEdit / QTextEdit.
///
/// Linear-scan strategy: a one-pass character walk identifies strings,
/// triple-quoted strings (both `"""` and `'''`) and `#` comments — these
/// regions are formatted directly and excluded from keyword/builtin/number
/// matching. This fixes the long-standing bug where a `"""` inside a
/// comment or single-line string would put the whole rest of the file
/// into "docstring" colour until another `"""` showed up.
///
/// Block state encodes the open triple-quote, if any:
///   0 — code
///   1 — inside `"""`
///   2 — inside `'''`
class PythonHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit PythonHighlighter(QTextDocument* parent = nullptr);

    /// Replace the active colour scheme and re-highlight the document.
    void setTheme(const PythonHighlightTheme& theme);

    /// Switch to a built-in preset by name (`Default`, `Monokai`,
    /// `Solarized Dark`, `Solarized Light`). Unknown names fall back to
    /// `Default`. Re-highlights the document.
    void setThemeByName(const QString& name);

    /// Get the active theme.
    const PythonHighlightTheme& theme() const { return theme_; }

    /// Built-in preset lookup. Returns the `Default` theme for an unknown
    /// name. The list of preset names is exposed via presetNames().
    static PythonHighlightTheme themeByName(const QString& name);
    static QStringList presetNames();

protected:
    void highlightBlock(const QString& text) override;

private:
    void rebuildFormats();
    void applyRulesIn(const QString& text, int start, int length);

    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    PythonHighlightTheme theme_;
    std::vector<Rule> rules_;
    QTextCharFormat stringFmt_;
    QTextCharFormat commentFmt_;
    QRegularExpression keywordRe_;
    QRegularExpression builtinRe_;
    QRegularExpression numberRe_;
    QRegularExpression decoratorRe_;
};

} // namespace microbotica::panels
