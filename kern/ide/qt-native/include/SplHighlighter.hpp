#pragma once

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>

class SplHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit SplHighlighter(QTextDocument* parent = nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    QVector<Rule> rules_;
    QTextCharFormat commentFormat_;
};
