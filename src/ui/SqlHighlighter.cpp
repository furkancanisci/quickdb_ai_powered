#include "ui/SqlHighlighter.h"

#include <QColor>
#include <QFont>
#include <QRegularExpression>
#include <QTextCharFormat>

#include "ui/SqlKeywords.h"

namespace ui {

namespace {

QStringList aggregateList()
{
    return {
        QStringLiteral("count"), QStringLiteral("sum"), QStringLiteral("avg"),
        QStringLiteral("min"), QStringLiteral("max"), QStringLiteral("group_concat"),
        QStringLiteral("coalesce"), QStringLiteral("ifnull"), QStringLiteral("nullif"),
        QStringLiteral("round"), QStringLiteral("abs"), QStringLiteral("length"),
        QStringLiteral("substr"), QStringLiteral("lower"), QStringLiteral("upper"),
        QStringLiteral("trim"), QStringLiteral("replace"), QStringLiteral("total"),
        QStringLiteral("row_number"), QStringLiteral("rank"), QStringLiteral("dense_rank"),
        QStringLiteral("lag"), QStringLiteral("lead"), QStringLiteral("string_agg"),
        QStringLiteral("now"), QStringLiteral("strftime")
    };
}

} // namespace

SqlHighlighter::SqlHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document)
{
    buildRules();
    m_multiLineComment = QRegularExpression(QStringLiteral("/\\*.*\\*/"));
}

void SqlHighlighter::setTheme(bool dark)
{
    if (m_dark == dark)
        return;
    m_dark = dark;
    rehighlight();
}

QTextCharFormat SqlHighlighter::formatFor(Kind kind) const
{
    QTextCharFormat f;
    switch (kind) {
    case Kind::Keyword:
        f.setForeground(m_dark ? QColor(0x7a, 0xa2, 0xff) : QColor(0x0b, 0x57, 0xd0));
        f.setFontWeight(QFont::Bold);
        break;
    case Kind::Special:
        f.setForeground(m_dark ? QColor(0xf2, 0x8b, 0x38) : QColor(0xa3, 0x35, 0x00));
        f.setFontWeight(QFont::Bold);
        break;
    case Kind::Type:
        f.setForeground(m_dark ? QColor(0x5c, 0xd6, 0xb0) : QColor(0x0f, 0x7b, 0x6c));
        break;
    case Kind::Function:
        f.setForeground(m_dark ? QColor(0xc9, 0xa0, 0xff) : QColor(0x6f, 0x42, 0xc1));
        break;
    case Kind::Number:
        f.setForeground(m_dark ? QColor(0xff, 0xd0, 0x8a) : QColor(0xb4, 0x53, 0x09));
        break;
    case Kind::String:
        f.setForeground(m_dark ? QColor(0x7e, 0xe7, 0x87) : QColor(0x0f, 0x6e, 0x2d));
        break;
    case Kind::Operator:
        f.setForeground(m_dark ? QColor(0xda, 0xaa, 0xee) : QColor(0x9a, 0x30, 0xa0));
        break;
    case Kind::Comment:
        f.setForeground(m_dark ? QColor(0x9a, 0xa0, 0xa6) : QColor(0x5c, 0x65, 0x70));
        f.setFontItalic(true);
        break;
    }
    return f;
}

void SqlHighlighter::buildRules()
{
    m_rules.clear();

    auto addWords = [this](const QStringList &words, Kind kind) {
        for (const QString &w : words) {
            Rule r;
            r.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(w),
                                            QRegularExpression::CaseInsensitiveOption);
            r.kind = kind;
            m_rules.append(r);
        }
    };

    addWords(sqlKeywordList(), Kind::Keyword);
    addWords(sqlSpecialList(), Kind::Special);
    addWords(sqlTypeList(), Kind::Type);
    addWords(aggregateList(), Kind::Function);

    // Functions: identifier directly before "(" (added after keywords so it
    // wins the later-rule-override race for names like COUNT(...)).
    {
        Rule r;
        r.pattern = QRegularExpression(QStringLiteral("\\b[a-zA-Z_][a-zA-Z0-9_]*(?=\\s*\\()"));
        r.kind = Kind::Function;
        m_rules.append(r);
    }

    // Numbers (int, decimal, exponent).
    {
        Rule r;
        r.pattern = QRegularExpression(QStringLiteral("\\b\\d+(\\.\\d+)?([eE][+-]?\\d+)?\\b"));
        r.kind = Kind::Number;
        m_rules.append(r);
    }

    // String literals: '...' (with '' escape) and "..." — these also shield
    // the keyword rules because they are appended last per-position, and the
    // comment pass below re-asserts comment colors over everything.
    {
        Rule r;
        r.pattern = QRegularExpression(QStringLiteral("'(?:[^']|'')*'"));
        r.kind = Kind::String;
        m_rules.append(r);
    }
    {
        Rule r;
        r.pattern = QRegularExpression(QStringLiteral("\"(?:[^\"]|\"\")*\""));
        r.kind = Kind::String;
        m_rules.append(r);
    }

    // Operators / comparison symbols.
    {
        Rule r;
        r.pattern = QRegularExpression(QStringLiteral("<=|>=|<>|!=|=|\\|\\||[*+/%-](?=[^\\s]*[a-zA-Z0-9'\"])"));
        r.kind = Kind::Operator;
        m_rules.append(r);
    }

    // Line comments.
    {
        Rule r;
        r.pattern = QRegularExpression(QStringLiteral("--[^\\n]*"));
        r.kind = Kind::Comment;
        m_rules.append(r);
    }
}

void SqlHighlighter::highlightBlock(const QString &text)
{
    for (const Rule &rule : m_rules) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        const QTextCharFormat fmt = formatFor(rule.kind);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), fmt);
        }
    }

    // Multi-line block comments spanning several blocks (win over everything).
    m_commentFormat = formatFor(Kind::Comment);
    int startIndex = text.indexOf(QStringLiteral("/*"));
    int previousState = previousBlockState();
    if (previousState != 0 && startIndex < 0) {
        // Comment continues from a previous block.
        setFormat(0, text.length(), m_commentFormat);
        int end = text.indexOf(QStringLiteral("*/"));
        if (end < 0)
            setCurrentBlockState(1);
        return;
    }
    while (startIndex >= 0) {
        const int endIndex = text.indexOf(QStringLiteral("*/"), startIndex + 2);
        int commentLength;
        if (endIndex == -1) {
            commentLength = text.length() - startIndex;
            setCurrentBlockState(1);
        } else {
            commentLength = endIndex - startIndex + 2;
        }
        setFormat(startIndex, commentLength, m_commentFormat);
        startIndex = text.indexOf(QStringLiteral("/*"), startIndex + commentLength);
    }
}

} // namespace ui
