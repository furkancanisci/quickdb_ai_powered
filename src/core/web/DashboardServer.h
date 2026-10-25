#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class QTcpServer;
class QTcpSocket;

namespace core {

/// A tiny, self-contained HTTP server (127.0.0.1 only) that renders the result
/// sets pinned from the desktop app as a browsable web dashboard. Each pinned
/// result becomes a "panel": a titled page with its data table plus a simple
/// auto-generated bar chart. No external assets, no JavaScript framework — the
/// whole UI is server-side HTML so it works fully offline on localhost.
class DashboardServer : public QObject {
    Q_OBJECT

public:
    /// One pinned result page.
    struct Panel {
        QString id;
        QString title;
        QString sql;
        QStringList columns;
        QVector<QVector<QString>> rows;
        QDateTime createdAt;
    };

    explicit DashboardServer(QObject *parent = nullptr);
    ~DashboardServer() override;

    /// Start listening. Prefers \a preferredPort; if busy, falls back to an
    /// ephemeral port. Returns true when a socket is listening. Idempotent.
    bool start(quint16 preferredPort = 8765);
    void stop();
    bool isRunning() const;
    quint16 port() const;
    /// Base URL to open in a browser, e.g. http://127.0.0.1:8765/
    QString baseUrl() const;

    /// Register a snapshot as a new panel; returns the generated panel id.
    /// An empty \a title is auto-derived from the SQL (e.g. the main table).
    QString addPanel(const QString &title, const QString &sql,
                     const QStringList &columns, const QVector<QVector<QString>> &rows);
    bool removePanel(const QString &id);
    bool hasPanel(const QString &id) const;
    QString panelTitle(const QString &id) const;
    int panelCount() const { return m_panels.size(); }
    bool isEmpty() const { return m_panels.isEmpty(); }

    /// Builds a short, human-readable panel title from a SQL statement by
    /// picking the main FROM/JOIN table (snake_case → Title Case). Falls back
    /// to a generic label when no table can be identified.
    static QString generateTitle(const QString &sql);

signals:
    void panelsChanged();

private:
    void onNewConnection();
    void handleClient(QTcpSocket *socket);
    QString renderPage(const QString &activeId) const;   // full HTML for one page
    QString renderIndex() const;                          // landing (first panel / empty)
    static QString escapeHtml(const QString &text);
    QString renderTable(const Panel &p) const;
    QString renderChart(const Panel &p) const;

    QTcpServer *m_server = nullptr;
    QVector<Panel> m_panels;
    int m_seq = 0;
};

} // namespace core
