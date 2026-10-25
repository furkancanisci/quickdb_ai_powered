#pragma once

#include <QPlainTextEdit>
#include <QWidget>

namespace ui {

class LineNumberArea; // defined at the bottom of this header
class SqlHighlighter;

/// Monospaced SQL editor with a line-number gutter, syntax highlighting and a
/// lightweight formatter. Ctrl+Enter emits executeRequested. Auto-completion is
/// intentionally disabled (its QCompleter popup crashed while typing).
class QueryEditor : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit QueryEditor(QWidget *parent = nullptr);

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();

    /// Returns the selected text, or the whole document if nothing selected.
    QString statementToExecute() const;

    /// Kept for API compatibility with MainWindow::refreshSchema. Auto-completion
    /// is disabled, so this is currently a no-op.
    void setSchemaCompletions(const QStringList &tokens);

    /// Reformats the whole document with the token-based SQL formatter.
    void formatSql();

    /// Token-based SQL pretty-printer (public for unit testing): clause
    /// keywords on their own lines, aligned AND/OR, comma-broken SELECT/SET
    /// lists, indented subqueries. String literals and comments are kept
    /// byte-for-byte intact.
    static QString formatSqlText(const QString &sql);

    /// Re-applies the active Theme palette to the syntax highlighter and
    /// line-number gutter. Called after every Theme::apply().
    void refreshTheme();

signals:
    void executeRequested();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect &rect, int dy);
    void highlightCurrentLine();

    LineNumberArea *m_lineNumberArea = nullptr;
    SqlHighlighter *m_highlighter = nullptr;
};

/// Gutter widget delegating to QueryEditor.
class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(QueryEditor *editor)
        : QWidget(editor), m_editor(editor) {}

    QSize sizeHint() const override { return QSize(m_editor->lineNumberAreaWidth(), 0); }

protected:
    void paintEvent(QPaintEvent *event) override { m_editor->lineNumberAreaPaintEvent(event); }

private:
    QueryEditor *m_editor;
};

} // namespace ui
