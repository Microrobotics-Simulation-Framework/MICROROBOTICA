#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <vector>

namespace microbotica::panels {

/// Basic Python syntax highlighter for QPlainTextEdit.
///
/// Highlights: keywords, builtins, strings, comments, numbers, decorators.
/// Sufficient for params.py/controller.py/setup.py editing — not a full
/// Python IDE. Users who want richer highlighting can use VS Code.
class PythonHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit PythonHighlighter(QTextDocument* parent = nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    std::vector<Rule> rules_;
    QTextCharFormat multiLineStringFormat_;
    QRegularExpression tripleQuoteStart_;
    QRegularExpression tripleQuoteEnd_;
};

} // namespace microbotica::panels
