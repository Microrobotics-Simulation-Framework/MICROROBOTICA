#include "panels/python_highlighter.h"

#include <QFont>
#include <QSettings>

namespace microbotica::panels {

namespace {

// Single keyword/builtin regex (alternation) — one pass instead of N.
const char* const kKeywordWords =
    "False|None|True|and|as|assert|async|await|break|class|continue|"
    "def|del|elif|else|except|finally|for|from|global|if|import|in|is|"
    "lambda|nonlocal|not|or|pass|raise|return|try|while|with|yield";

const char* const kBuiltinWords =
    "print|len|range|int|float|str|list|dict|tuple|set|type|isinstance|"
    "super|open|enumerate|zip|map|filter|self|cls|abs|min|max|sum|sorted|"
    "reversed|any|all|bool|bytes|bytearray|callable|getattr|setattr|"
    "hasattr|delattr|id|repr|hash|input|format";

PythonHighlightTheme defaultTheme() {
    return PythonHighlightTheme{
        QStringLiteral("Default"),
        QColor(198, 120, 221),   // keyword — purple
        QColor(97, 175, 239),    // builtin — blue
        QColor(152, 195, 121),   // string — green
        QColor(128, 128, 128),   // comment — gray
        QColor(209, 154, 102),   // number — orange
        QColor(229, 192, 123),   // decorator — yellow
        /*keywordBold=*/true, /*commentItalic=*/true};
}

PythonHighlightTheme monokaiTheme() {
    return PythonHighlightTheme{
        QStringLiteral("Monokai"),
        QColor(0xF9, 0x26, 0x72),  // keyword — magenta
        QColor(0x66, 0xD9, 0xEF),  // builtin — cyan
        QColor(0xE6, 0xDB, 0x74),  // string — yellow
        QColor(0x75, 0x71, 0x5E),  // comment — muted brown
        QColor(0xAE, 0x81, 0xFF),  // number — purple
        QColor(0xA6, 0xE2, 0x2E),  // decorator — green
        true, true};
}

PythonHighlightTheme solarizedDarkTheme() {
    return PythonHighlightTheme{
        QStringLiteral("Solarized Dark"),
        QColor(0x85, 0x99, 0x00),  // keyword — green
        QColor(0x26, 0x8B, 0xD2),  // builtin — blue
        QColor(0x2A, 0xA1, 0x98),  // string — cyan
        QColor(0x58, 0x6E, 0x75),  // comment — base01
        QColor(0xD3, 0x36, 0x82),  // number — magenta
        QColor(0xB5, 0x89, 0x00),  // decorator — yellow
        true, true};
}

PythonHighlightTheme solarizedLightTheme() {
    return PythonHighlightTheme{
        QStringLiteral("Solarized Light"),
        QColor(0x85, 0x99, 0x00),  // keyword — green
        QColor(0x26, 0x8B, 0xD2),  // builtin — blue
        QColor(0x2A, 0xA1, 0x98),  // string — cyan
        QColor(0x93, 0xA1, 0xA1),  // comment — base1
        QColor(0xD3, 0x36, 0x82),  // number — magenta
        QColor(0xB5, 0x89, 0x00),  // decorator — yellow
        true, true};
}

} // namespace

QStringList PythonHighlighter::presetNames() {
    return {QStringLiteral("Default"),
            QStringLiteral("Monokai"),
            QStringLiteral("Solarized Dark"),
            QStringLiteral("Solarized Light")};
}

PythonHighlightTheme PythonHighlighter::themeByName(const QString& name) {
    if (name == QLatin1String("Monokai"))         return monokaiTheme();
    if (name == QLatin1String("Solarized Dark"))  return solarizedDarkTheme();
    if (name == QLatin1String("Solarized Light")) return solarizedLightTheme();
    return defaultTheme();
}

PythonHighlighter::PythonHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    // Compile the alternation regexes once.
    keywordRe_ = QRegularExpression(
        QStringLiteral("\\b(?:%1)\\b").arg(QLatin1String(kKeywordWords)));
    builtinRe_ = QRegularExpression(
        QStringLiteral("\\b(?:%1)\\b").arg(QLatin1String(kBuiltinWords)));
    numberRe_ = QRegularExpression(
        QStringLiteral("\\b[0-9]+(?:\\.[0-9]+)?(?:[eE][+-]?[0-9]+)?\\b"));
    decoratorRe_ = QRegularExpression(QStringLiteral("@\\w+"));

    // Initial theme — read the persisted preference if any.
    QSettings s;
    s.beginGroup("code_highlight");
    const QString themeName = s.value("theme", "Default").toString();
    s.endGroup();
    theme_ = themeByName(themeName);
    rebuildFormats();
}

void PythonHighlighter::setTheme(const PythonHighlightTheme& theme) {
    theme_ = theme;
    rebuildFormats();
    rehighlight();
}

void PythonHighlighter::setThemeByName(const QString& name) {
    setTheme(themeByName(name));
}

void PythonHighlighter::rebuildFormats() {
    rules_.clear();

    QTextCharFormat keywordFmt;
    keywordFmt.setForeground(theme_.keyword);
    if (theme_.keywordBold) keywordFmt.setFontWeight(QFont::Bold);
    rules_.push_back({keywordRe_, keywordFmt});

    QTextCharFormat builtinFmt;
    builtinFmt.setForeground(theme_.builtin);
    rules_.push_back({builtinRe_, builtinFmt});

    QTextCharFormat decoratorFmt;
    decoratorFmt.setForeground(theme_.decorator);
    rules_.push_back({decoratorRe_, decoratorFmt});

    QTextCharFormat numberFmt;
    numberFmt.setForeground(theme_.number);
    rules_.push_back({numberRe_, numberFmt});

    stringFmt_ = QTextCharFormat();
    stringFmt_.setForeground(theme_.string);

    commentFmt_ = QTextCharFormat();
    commentFmt_.setForeground(theme_.comment);
    if (theme_.commentItalic) commentFmt_.setFontItalic(true);
}

void PythonHighlighter::applyRulesIn(const QString& text, int start, int length) {
    if (length <= 0) return;
    const QString sub = text.mid(start, length);
    for (const auto& rule : rules_) {
        auto it = rule.pattern.globalMatch(sub);
        while (it.hasNext()) {
            auto match = it.next();
            setFormat(start + static_cast<int>(match.capturedStart()),
                      static_cast<int>(match.capturedLength()),
                      rule.format);
        }
    }
}

void PythonHighlighter::highlightBlock(const QString& text) {
    setCurrentBlockState(0);

    // Mode tracks where we are in the line. Block state 1 means we entered
    // this line still inside `"""`; state 2 means inside `'''`. Otherwise
    // we begin in Code.
    enum Mode { Code, SingleSQ, SingleDQ, TripleDQ, TripleSQ };
    Mode mode = Code;
    int regionStart = -1;
    int codeStart = 0;

    const int prev = previousBlockState();
    if (prev == 1) { mode = TripleDQ; regionStart = 0; codeStart = -1; }
    else if (prev == 2) { mode = TripleSQ; regionStart = 0; codeStart = -1; }

    // Code spans accumulated for keyword/builtin/number/decorator passes.
    std::vector<std::pair<int, int>> codeSpans;
    auto closeCode = [&](int end) {
        if (codeStart >= 0 && end > codeStart) {
            codeSpans.emplace_back(codeStart, end - codeStart);
        }
        codeStart = -1;
    };

    const int n = text.length();
    int i = 0;

    while (i < n) {
        const QChar c = text[i];

        if (mode == Code) {
            // Triple-quoted strings — checked before single-quote so `"""`
            // is not mis-detected as an empty-string `""` followed by `"`.
            if (c == QLatin1Char('"') && i + 2 < n
                && text[i + 1] == QLatin1Char('"')
                && text[i + 2] == QLatin1Char('"')) {
                closeCode(i);
                mode = TripleDQ;
                regionStart = i;
                i += 3;
                continue;
            }
            if (c == QLatin1Char('\'') && i + 2 < n
                && text[i + 1] == QLatin1Char('\'')
                && text[i + 2] == QLatin1Char('\'')) {
                closeCode(i);
                mode = TripleSQ;
                regionStart = i;
                i += 3;
                continue;
            }
            if (c == QLatin1Char('"')) {
                closeCode(i);
                mode = SingleDQ;
                regionStart = i;
                ++i;
                continue;
            }
            if (c == QLatin1Char('\'')) {
                closeCode(i);
                mode = SingleSQ;
                regionStart = i;
                ++i;
                continue;
            }
            if (c == QLatin1Char('#')) {
                closeCode(i);
                setFormat(i, n - i, commentFmt_);
                i = n;
                break;
            }
            if (codeStart < 0) codeStart = i;
            ++i;
            continue;
        }

        if (mode == SingleDQ || mode == SingleSQ) {
            const QChar terminator = (mode == SingleDQ)
                ? QLatin1Char('"') : QLatin1Char('\'');
            if (c == QLatin1Char('\\') && i + 1 < n) {
                i += 2;
                continue;
            }
            if (c == terminator) {
                setFormat(regionStart, i - regionStart + 1, stringFmt_);
                regionStart = -1;
                mode = Code;
                codeStart = i + 1;
                ++i;
                continue;
            }
            ++i;
            continue;
        }

        // TripleDQ or TripleSQ — look for the matching three-quote run.
        const QChar q = (mode == TripleDQ)
            ? QLatin1Char('"') : QLatin1Char('\'');
        if (c == q && i + 2 < n && text[i + 1] == q && text[i + 2] == q) {
            setFormat(regionStart, i + 3 - regionStart, stringFmt_);
            regionStart = -1;
            mode = Code;
            codeStart = i + 3;
            i += 3;
            continue;
        }
        ++i;
    }

    // End of line — close out the current region.
    switch (mode) {
        case Code:
            closeCode(n);
            break;
        case SingleDQ:
        case SingleSQ:
            // An unterminated single-line string at EOL is still styled
            // for what we have; Python would raise but the editor stays
            // legible. We do NOT carry the state into the next line.
            if (regionStart >= 0) {
                setFormat(regionStart, n - regionStart, stringFmt_);
            }
            break;
        case TripleDQ:
            if (regionStart >= 0) {
                setFormat(regionStart, n - regionStart, stringFmt_);
            }
            setCurrentBlockState(1);
            break;
        case TripleSQ:
            if (regionStart >= 0) {
                setFormat(regionStart, n - regionStart, stringFmt_);
            }
            setCurrentBlockState(2);
            break;
    }

    // Apply keyword/builtin/number/decorator rules within code spans.
    for (const auto& span : codeSpans) {
        applyRulesIn(text, span.first, span.second);
    }
}

} // namespace microbotica::panels
