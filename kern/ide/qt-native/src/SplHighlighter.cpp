#include "SplHighlighter.hpp"
#include <QColor>
#include <QFont>

SplHighlighter::SplHighlighter(QTextDocument* parent) : QSyntaxHighlighter(parent) {
    QTextCharFormat keyword;
    keyword.setForeground(QColor(90, 165, 255));
    keyword.setFontWeight(QFont::Bold);

    const QStringList keywords = {
        "let", "const", "var", "int", "float", "bool", "string", "void", "char", "long", "double",
        "if", "elif", "else", "for", "while", "do", "in", "range", "break", "continue", "repeat",
        "def", "fn", "function", "lambda", "return", "yield",
        "class", "enum", "constructor", "init", "extends", "new", "this", "super",
        "public", "private", "protected",
        "export", "import",
        "try", "catch", "finally", "throw", "rethrow", "defer", "assert",
        "match", "case", "with", "as",
        "and", "or", "true", "false", "null", "nil"
    };
    for (const QString& kw : keywords) {
        rules_.push_back({QRegularExpression(QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(kw))), keyword});
    }

    QTextCharFormat strFmt;
    strFmt.setForeground(QColor(232, 189, 95));
    rules_.push_back({QRegularExpression(QStringLiteral("\"([^\"\\\\]|\\\\.)*\"")), strFmt});
    rules_.push_back({QRegularExpression(QStringLiteral("'([^'\\\\]|\\\\.)*'")), strFmt});

    QTextCharFormat numFmt;
    numFmt.setForeground(QColor(152, 195, 121));
    rules_.push_back({QRegularExpression(QStringLiteral("\\b\\d+(\\.\\d+)?\\b")), numFmt});

    commentFormat_.setForeground(QColor(128, 136, 147));
}

void SplHighlighter::highlightBlock(const QString& text) {
    for (const Rule& r : rules_) {
        auto it = r.pattern.globalMatch(text);
        while (it.hasNext()) {
            const auto m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), r.format);
        }
    }

    const int c = text.indexOf("//");
    if (c >= 0) {
        setFormat(c, text.length() - c, commentFormat_);
    }
}
