#pragma once

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>

class QTextDocument;

namespace ui {

/// SQL syntax highlighter with an expanded, theme-aware color system:
/// keywords, data types, aggregates/window specials, functions, numbers,
/// string literals, line/block comments and operators. Colors switch with
/// the active Theme so text never looks washed out in either mode.
/// Attached to the QueryEditor document.
class SqlHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit SqlHighlighter(QTextDocument *document);

    /// Switches between the light/dark palettes and repaints the document.
    void setTheme(bool dark);

protected:
    void highlightBlock(const QString &text) override;

private:
    enum class Kind {
        Keyword,   // SELECT, FROM, WHERE ...
        Special,   // WITH, OVER, PARTITION, CASE branches, EXISTS ...
        Type,      // INTEGER, TEXT, VARCHAR ...
        Function,  // identifier directly before "("
        Number,
        String,
        Operator,
        Comment
    };

    struct Rule {
        QRegularExpression pattern;
        Kind kind;
    };

    QTextCharFormat formatFor(Kind kind) const;
    void buildRules();

    QVector<Rule> m_rules;
    bool m_dark = false;

    QRegularExpression m_multiLineComment; // /* ... */
    QTextCharFormat m_commentFormat;
};

} // namespace ui
