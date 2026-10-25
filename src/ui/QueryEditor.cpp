#include "ui/QueryEditor.h"

#include <QKeyEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QRegularExpression>
#include <QSet>
#include <QTextBlock>
#include <QTextCursor>

#include "ui/SqlHighlighter.h"
#include "ui/SqlKeywords.h"
#include "ui/Theme.h"

namespace ui {

QueryEditor::QueryEditor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    QFont font(QStringLiteral("Menlo"));
    if (!font.exactMatch())
        font = QFont(QStringLiteral("Monospace"));
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(13);
    setFont(font);

    setLineWrapMode(QPlainTextEdit::NoWrap);
    setTabStopDistance(4 * fontMetrics().horizontalAdvance(QLatin1Char(' ')));
    setPlaceholderText(QStringLiteral("Enter SQL here... (Ctrl+Enter to execute)"));

    m_lineNumberArea = new LineNumberArea(this);

    // Syntax highlighting attached to the document.
    m_highlighter = new SqlHighlighter(document());

    // Auto-completion is intentionally disabled: the SQL editor's QCompleter
    // popup crashed while typing (QCompleterPrivate::showPopup dereferenced a
    // null completion widget on Homebrew's Qt 6.9 build), so no completer,
    // model or popup is created here at all.
    connect(this, &QueryEditor::blockCountChanged, this, &QueryEditor::updateLineNumberAreaWidth);
    connect(this, &QueryEditor::updateRequest, this, &QueryEditor::updateLineNumberArea);
    connect(this, &QueryEditor::cursorPositionChanged, this, &QueryEditor::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();

    m_highlighter->setTheme(Theme::current() == Theme::Mode::Dark);
}

void QueryEditor::refreshTheme()
{
    if (m_highlighter)
        m_highlighter->setTheme(Theme::current() == Theme::Mode::Dark);
    highlightCurrentLine();
    if (m_lineNumberArea)
        m_lineNumberArea->update();
}

int QueryEditor::lineNumberAreaWidth()
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    return 13 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}

void QueryEditor::updateLineNumberAreaWidth(int /*newBlockCount*/)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void QueryEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        m_lineNumberArea->scroll(0, dy);
    else
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void QueryEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    const QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void QueryEditor::keyPressEvent(QKeyEvent *event)
{
    // Ctrl+Enter (Cmd+Enter on macOS) executes the script.
    if ((event->modifiers() & (Qt::ControlModifier | Qt::MetaModifier)) &&
        (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        emit executeRequested();
        return;
    }
    // Plain Tab inserts spaces instead of moving focus.
    if (event->key() == Qt::Key_Tab) {
        insertPlainText(QStringLiteral("    "));
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
}


void QueryEditor::setSchemaCompletions(const QStringList &tokens)
{
    // Auto-completion is disabled; kept as a no-op so MainWindow::refreshSchema
    // can keep feeding tokens without wiring up a completion popup.
    Q_UNUSED(tokens);
}

namespace {

// --- Token-based SQL formatter ----------------------------------------
struct FTok {
    enum Kind { Word, Num, Str, Sym, LP, RP, Comma, Semi, LineCmt, BlockCmt };
    Kind kind;
    QString text;
    bool glued = false; // no whitespace before this token in the source
};

QVector<FTok> sqlTokenize(const QString &sql)
{
    QVector<FTok> toks;
    const int n = sql.size();
    int i = 0;
    auto isWs = [n, &sql](int p) {
        return p < n && (sql.at(p) == u' ' || sql.at(p) == u'\t' ||
                         sql.at(p) == u'\n' || sql.at(p) == u'\r');
    };
    while (i < n) {
        if (isWs(i)) {
            ++i;
            continue;
        }
        const bool glued = !toks.isEmpty() && !isWs(i - 1);
        if (sql.mid(i, 2) == QStringLiteral("--")) {
            int e = sql.indexOf(u'\n', i);
            if (e < 0)
                e = n;
            toks.append({FTok::LineCmt, sql.mid(i, e - i).trimmed(), glued});
            i = e;
            continue;
        }
        if (sql.mid(i, 2) == QStringLiteral("/*")) {
            const int e = sql.indexOf(QStringLiteral("*/"), i + 2);
            const int stop = e < 0 ? n : e + 2;
            toks.append({FTok::BlockCmt, sql.mid(i, stop - i), glued});
            i = stop;
            continue;
        }
        const QChar c = sql.at(i);
        if (c == u'(') { toks.append({FTok::LP, QStringLiteral("("), glued}); ++i; continue; }
        if (c == u')') { toks.append({FTok::RP, QStringLiteral(")"), glued}); ++i; continue; }
        if (c == u',') { toks.append({FTok::Comma, QStringLiteral(","), glued}); ++i; continue; }
        if (c == u';') { toks.append({FTok::Semi, QStringLiteral(";"), glued}); ++i; continue; }
        if (c == u'\'' || c == u'"') {
            int j = i + 1;
            while (j < n) {
                if (sql.at(j) == c) {
                    if (j + 1 < n && sql.at(j + 1) == c) { // doubled quote escape
                        j += 2;
                        continue;
                    }
                    ++j;
                    break;
                }
                ++j;
            }
            toks.append({FTok::Str, sql.mid(i, j - i), glued});
            i = j;
            continue;
        }
        if (c.isDigit()) {
            int j = i;
            while (j < n && (sql.at(j).isLetterOrNumber() || sql.at(j) == u'.'))
                ++j;
            toks.append({FTok::Num, sql.mid(i, j - i), glued});
            i = j;
            continue;
        }
        if (c.isLetter() || c == u'_' || c == u'$') {
            int j = i;
            while (j < n && (sql.at(j).isLetterOrNumber() || sql.at(j) == u'_' ||
                             sql.at(j) == u'$'))
                ++j;
            toks.append({FTok::Word, sql.mid(i, j - i), glued});
            i = j;
            continue;
        }
        if (sql.mid(i, 2) == QStringLiteral("<>") ||
            sql.mid(i, 2) == QStringLiteral("<=") ||
            sql.mid(i, 2) == QStringLiteral(">=") ||
            sql.mid(i, 2) == QStringLiteral("!=") ||
            sql.mid(i, 2) == QStringLiteral("||")) {
            toks.append({FTok::Sym, sql.mid(i, 2), glued});
            i += 2;
            continue;
        }
        toks.append({FTok::Sym, QString(c), glued});
        ++i;
    }
    return toks;
}

} // namespace

QString QueryEditor::formatSqlText(const QString &sql)
{
    const QVector<FTok> toks = sqlTokenize(sql);
    if (toks.isEmpty())
        return sql.trimmed();

    static const QSet<QString> clauseWords = {
        QStringLiteral("SELECT"), QStringLiteral("FROM"), QStringLiteral("WHERE"),
        QStringLiteral("GROUP"), QStringLiteral("HAVING"), QStringLiteral("ORDER"),
        QStringLiteral("LIMIT"), QStringLiteral("OFFSET"), QStringLiteral("UNION"),
        QStringLiteral("INTERSECT"), QStringLiteral("EXCEPT"), QStringLiteral("INSERT"),
        QStringLiteral("UPDATE"), QStringLiteral("DELETE"), QStringLiteral("SET"),
        QStringLiteral("VALUES"), QStringLiteral("RETURNING"), QStringLiteral("WITH"),
        QStringLiteral("FETCH")
    };
    static const QSet<QString> joinWords = {
        QStringLiteral("JOIN"), QStringLiteral("INNER"), QStringLiteral("LEFT"),
        QStringLiteral("RIGHT"), QStringLiteral("FULL"), QStringLiteral("CROSS"),
        QStringLiteral("NATURAL")
    };
    static const QSet<QString> caseLineWords = {
        QStringLiteral("WHEN"), QStringLiteral("ELSE"), QStringLiteral("END")
    };
    QSet<QString> kwSet;
    for (const QString &k : sqlKeywordList())
        kwSet.insert(k.toUpper());
    for (const QString &k : sqlSpecialList())
        kwSet.insert(k.toUpper());
    for (const QString &k : sqlTypeList())
        kwSet.insert(k.toUpper());
    static const QSet<QString> ddlStartWords = {
        QStringLiteral("INTEGER"), QStringLiteral("INT"), QStringLiteral("TEXT"),
        QStringLiteral("REAL"), QStringLiteral("BLOB"), QStringLiteral("NUMERIC"),
        QStringLiteral("BOOLEAN"), QStringLiteral("DATE"), QStringLiteral("DATETIME"),
        QStringLiteral("TIMESTAMP"), QStringLiteral("VARCHAR"), QStringLiteral("SERIAL"),
        QStringLiteral("BIGINT"), QStringLiteral("UUID"), QStringLiteral("JSONB"),
        QStringLiteral("PRIMARY"), QStringLiteral("NOT"), QStringLiteral("UNIQUE"),
        QStringLiteral("DEFAULT"), QStringLiteral("CONSTRAINT"), QStringLiteral("CHECK"),
        QStringLiteral("REFERENCES"), QStringLiteral("KEY")
    };

    QString out;
    int blockDepth = 0;
    QVector<QPair<bool, QString>> parenStack; // {inline-paren, clauseBefore}
    QString curClause;                        // last clause-ish word (upper)
    QString prevWord;                         // last Word token (upper)

    auto atLineStart = [&out]() {
        return out.isEmpty() || out.endsWith(u'\n');
    };
    auto startLine = [&](int extra) {
        if (!atLineStart())
            out += u'\n';
        out += QString(2 * (blockDepth + extra), u' ');
    };
    auto appendInline = [&](const QString &t) {
        if (atLineStart()) {
            out += QString(2 * blockDepth, u' ');
            out += t;
            return;
        }
        // Keep dotted identifiers glued: "t1.id", not "t1. id".
        const bool noSpaceBefore = out.isEmpty() || out.endsWith(u' ') ||
                                   out.endsWith(u'(') || out.endsWith(u'.');
        if (!noSpaceBefore && t != QStringLiteral("."))
            out += u' ';
        out += t;
    };

    for (int idx = 0; idx < toks.size(); ++idx) {
        const FTok &t = toks.at(idx);
        const bool inlineCtx = !parenStack.isEmpty() && parenStack.last().first;

        switch (t.kind) {
        case FTok::Word: {
            const QString U = t.text.toUpper();
            if (!inlineCtx && clauseWords.contains(U)) {
                const bool continuation =
                    (U == QStringLiteral("BY") &&
                     (prevWord == QStringLiteral("GROUP") || prevWord == QStringLiteral("ORDER"))) ||
                    (U == QStringLiteral("INTO") && prevWord == QStringLiteral("INSERT")) ||
                    (U == QStringLiteral("FROM") && prevWord == QStringLiteral("DELETE")) ||
                    (U == QStringLiteral("SET") && prevWord == QStringLiteral("ON")) ||
                    (U == QStringLiteral("ALL") &&
                     (prevWord == QStringLiteral("UNION") || prevWord == QStringLiteral("EXCEPT") ||
                      prevWord == QStringLiteral("INTERSECT") || prevWord == QStringLiteral("DISTINCT"))) ||
                    (U == QStringLiteral("DISTINCT") && prevWord == QStringLiteral("SELECT")) ||
                    U == QStringLiteral("OUTER") ||
                    U == QStringLiteral("REPLACE"); // ON CONFLICT REPLACE / INSERT OR REPLACE
                if (!continuation) {
                    startLine(0);
                    out += U;
                } else {
                    appendInline(U);
                }
                curClause = U;
            } else if (!inlineCtx && joinWords.contains(U)) {
                if (prevWord.isEmpty() || !joinWords.contains(prevWord)) {
                    startLine(0);
                    out += U;
                } else {
                    appendInline(U);
                }
            } else if (!inlineCtx &&
                       (U == QStringLiteral("AND") || U == QStringLiteral("OR"))) {
                startLine(1);
                out += U;
            } else if (!inlineCtx && caseLineWords.contains(U)) {
                startLine(U == QStringLiteral("END") ? 0 : 1);
                out += U;
            } else {
                appendInline(kwSet.contains(U) ? U : t.text);
            }
            prevWord = U;
            if (U == QStringLiteral("TABLE") &&
                (curClause.isEmpty() || curClause == QStringLiteral("CREATE") ||
                 curClause == QStringLiteral("ALTER")))
                curClause = QStringLiteral("TABLE");
            break;
        }
        case FTok::LP: {
            // Classify: function parens stay inline; subqueries and column
            // definition lists become block (multi-line, indented) parens.
            const bool func = t.glued && !prevWord.isEmpty() && !kwSet.contains(prevWord);
            bool subquery = false;
            bool ddlList = false;
            if (!func) {
                // Look ahead at the first two significant inner tokens.
                const FTok *first = nullptr;
                const FTok *second = nullptr;
                for (int j = idx + 1; j < toks.size(); ++j) {
                    if (toks[j].kind == FTok::LineCmt || toks[j].kind == FTok::BlockCmt)
                        continue;
                    if (toks[j].kind == FTok::RP)
                        break; // empty parens
                    if (!first) {
                        first = &toks[j];
                        continue;
                    }
                    second = &toks[j];
                    break;
                }
                if (first && first->kind == FTok::Word) {
                    const QString w1 = first->text.toUpper();
                    if (clauseWords.contains(w1) && w1 != QStringLiteral("SET"))
                        subquery = true;
                    if (!subquery && second && second->kind == FTok::Word &&
                        ddlStartWords.contains(second->text.toUpper()))
                        ddlList = true;
                }
            }
            const bool block = !func && (subquery ||
                                         (ddlList && curClause == QStringLiteral("TABLE")));
            parenStack.append({!block, curClause});
            if (t.glued && !out.isEmpty() && !atLineStart())
                out += u'(';
            else
                appendInline(QStringLiteral("("));
            if (block) {
                ++blockDepth;
                out += u'\n';
                if (subquery)
                    curClause.clear();
            }
            break;
        }
        case FTok::RP: {
            const bool wasInline = parenStack.isEmpty() ? true : parenStack.last().first;
            const QString clauseBefore =
                parenStack.isEmpty() ? curClause : parenStack.last().second;
            if (!parenStack.isEmpty())
                parenStack.removeLast();
            if (wasInline) {
                if (!out.isEmpty() && !out.endsWith(u' ') && !atLineStart())
                    out += u')';
                else
                    appendInline(QStringLiteral(")"));
            } else {
                blockDepth = qMax(0, blockDepth - 1);
                if (!atLineStart())
                    out += u'\n';
                out += QString(2 * blockDepth, u' ');
                out += u')';
            }
            if (!clauseBefore.isEmpty())
                curClause = clauseBefore;
            break;
        }
        case FTok::Comma: {
            if (!inlineCtx && (curClause == QStringLiteral("SELECT") ||
                               curClause == QStringLiteral("SET") ||
                               curClause == QStringLiteral("VALUES") ||
                               curClause == QStringLiteral("TABLE") ||
                               curClause == QStringLiteral("WITH"))) {
                out += u',';
                startLine(1);
            } else {
                out += QStringLiteral(", ");
            }
            break;
        }
        case FTok::Semi: {
            out += u';';
            parenStack.clear();
            blockDepth = 0;
            curClause.clear();
            prevWord.clear();
            if (idx + 1 < toks.size())
                out += QStringLiteral("\n\n");
            break;
        }
        case FTok::LineCmt: {
            if (!atLineStart())
                out += u'\n';
            out += QString(2 * blockDepth, u' ');
            out += t.text;
            out += u'\n';
            break;
        }
        case FTok::BlockCmt:
            appendInline(t.text);
            break;
        case FTok::Num:
        case FTok::Str:
        case FTok::Sym:
            appendInline(t.text);
            break;
        }
    }

    // Trim trailing spaces off every line before returning.
    QStringList lines = out.split(u'\n');
    for (QString &ln : lines) {
        while (ln.endsWith(u' '))
            ln.chop(1);
    }
    QString result = lines.join(u'\n');
    while (result.endsWith(u'\n'))
        result.chop(1);
    return result;
}

void QueryEditor::formatSql()
{
    const QString formatted = formatSqlText(toPlainText());
    if (!formatted.isEmpty())
        setPlainText(formatted);
}

QString QueryEditor::statementToExecute() const
{
    const QString selected = textCursor().selectedText();
    if (!selected.trimmed().isEmpty())
        return selected;
    return toPlainText();
}

void QueryEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> selections;
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        // Palette-driven so the band stays subtle in both themes.
        const QColor bg = (Theme::current() == Theme::Mode::Dark)
                              ? QColor(0x2a, 0x33, 0x45)
                              : QColor(0xe8, 0xf0, 0xfe);
        selection.format.setBackground(bg);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        selections.append(selection);
    }
    setExtraSelections(selections);
}

void QueryEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(m_lineNumberArea);
    const bool dark = Theme::current() == Theme::Mode::Dark;
    painter.fillRect(event->rect(),
                     dark ? QColor(0x1b, 0x1c, 0x22) : QColor(0xf5, 0xf6, 0xf8));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            const QString number = QString::number(blockNumber + 1);
            painter.setPen(blockNumber == textCursor().blockNumber()
                               ? (dark ? QColor(0x93, 0xb8, 0xff) : QColor(0x1d, 0x4e, 0xd8))
                                      : (dark ? QColor(0x8b, 0x93, 0xa3) : QColor(0x8c, 0x94, 0x9e)));
            painter.drawText(0, top, m_lineNumberArea->width() - 5,
                             fontMetrics().height(), Qt::AlignRight, number);
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

} // namespace ui
