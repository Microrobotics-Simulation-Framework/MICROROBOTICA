#include "panels/python_highlighter.h"

namespace microbotica::panels {

PythonHighlighter::PythonHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    // Keywords
    QTextCharFormat keywordFmt;
    keywordFmt.setForeground(QColor(198, 120, 221)); // purple
    keywordFmt.setFontWeight(QFont::Bold);

    const QStringList keywords = {
        "\\bFalse\\b", "\\bNone\\b", "\\bTrue\\b", "\\band\\b", "\\bas\\b",
        "\\bassert\\b", "\\basync\\b", "\\bawait\\b", "\\bbreak\\b", "\\bclass\\b",
        "\\bcontinue\\b", "\\bdef\\b", "\\bdel\\b", "\\belif\\b", "\\belse\\b",
        "\\bexcept\\b", "\\bfinally\\b", "\\bfor\\b", "\\bfrom\\b", "\\bglobal\\b",
        "\\bif\\b", "\\bimport\\b", "\\bin\\b", "\\bis\\b", "\\blambda\\b",
        "\\bnonlocal\\b", "\\bnot\\b", "\\bor\\b", "\\bpass\\b", "\\braise\\b",
        "\\breturn\\b", "\\btry\\b", "\\bwhile\\b", "\\bwith\\b", "\\byield\\b",
    };
    for (const auto& kw : keywords) {
        rules_.push_back({QRegularExpression(kw), keywordFmt});
    }

    // Builtins
    QTextCharFormat builtinFmt;
    builtinFmt.setForeground(QColor(97, 175, 239)); // blue

    const QStringList builtins = {
        "\\bprint\\b", "\\blen\\b", "\\brange\\b", "\\bint\\b", "\\bfloat\\b",
        "\\bstr\\b", "\\blist\\b", "\\bdict\\b", "\\btuple\\b", "\\bset\\b",
        "\\btype\\b", "\\bisinstance\\b", "\\bsuper\\b", "\\bopen\\b",
        "\\benumerate\\b", "\\bzip\\b", "\\bmap\\b", "\\bfilter\\b",
        "\\bself\\b",
    };
    for (const auto& bi : builtins) {
        rules_.push_back({QRegularExpression(bi), builtinFmt});
    }

    // Decorators
    QTextCharFormat decoratorFmt;
    decoratorFmt.setForeground(QColor(229, 192, 123)); // yellow
    rules_.push_back({QRegularExpression("@\\w+"), decoratorFmt});

    // Numbers
    QTextCharFormat numberFmt;
    numberFmt.setForeground(QColor(209, 154, 102)); // orange
    rules_.push_back({QRegularExpression("\\b[0-9]+(\\.[0-9]+)?([eE][+-]?[0-9]+)?\\b"), numberFmt});

    // Single-line strings (single and double quotes)
    QTextCharFormat stringFmt;
    stringFmt.setForeground(QColor(152, 195, 121)); // green
    rules_.push_back({QRegularExpression("\"[^\"\\\\]*(\\\\.[^\"\\\\]*)*\""), stringFmt});
    rules_.push_back({QRegularExpression("'[^'\\\\]*(\\\\.[^'\\\\]*)*'"), stringFmt});

    // Comments
    QTextCharFormat commentFmt;
    commentFmt.setForeground(QColor(128, 128, 128)); // gray
    commentFmt.setFontItalic(true);
    rules_.push_back({QRegularExpression("#[^\n]*"), commentFmt});

    // Multi-line strings (triple quotes)
    multiLineStringFormat_ = stringFmt;
    tripleQuoteStart_ = QRegularExpression("\"\"\"");
    tripleQuoteEnd_ = QRegularExpression("\"\"\"");
}

void PythonHighlighter::highlightBlock(const QString& text) {
    // Apply single-line rules
    for (const auto& rule : rules_) {
        auto it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            auto match = it.next();
            setFormat(static_cast<int>(match.capturedStart()),
                      static_cast<int>(match.capturedLength()),
                      rule.format);
        }
    }

    // Multi-line string handling (triple double quotes)
    setCurrentBlockState(0);

    int startIndex = 0;
    if (previousBlockState() != 1) {
        auto match = tripleQuoteStart_.match(text);
        startIndex = match.hasMatch() ? static_cast<int>(match.capturedStart()) : -1;
    }

    while (startIndex >= 0) {
        auto endMatch = tripleQuoteEnd_.match(text, startIndex + 3);
        int endIndex = endMatch.hasMatch() ? static_cast<int>(endMatch.capturedStart()) : -1;

        int length;
        if (endIndex == -1) {
            setCurrentBlockState(1);
            length = text.length() - startIndex;
        } else {
            length = endIndex - startIndex + 3;
        }

        setFormat(startIndex, length, multiLineStringFormat_);

        auto nextMatch = tripleQuoteStart_.match(text, startIndex + length);
        startIndex = nextMatch.hasMatch() ? static_cast<int>(nextMatch.capturedStart()) : -1;
    }
}

} // namespace microbotica::panels
