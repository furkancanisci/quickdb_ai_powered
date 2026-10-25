#include "core/web/DashboardServer.h"

#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

#include <memory>

namespace core {

namespace {

// A column is treated as a numeric metric when at least half of its non-empty
// values parse as a plain number. Identifiers named like keys/ids are ignored
// for the chart value so we never plot a surrogate key as a metric.
bool parseNumber(const QString &s, double *out)
{
    QString t = s.trimmed();
    if (t.isEmpty())
        return false;
    t.replace(QStringLiteral(","), QStringLiteral("."));
    t.remove(QLatin1Char(' '));
    bool ok = false;
    const double v = t.toDouble(&ok);
    if (ok)
        *out = v;
    return ok;
}

bool isIdentifierName(const QString &name)
{
    const QString h = name.toLower();
    if (h == QLatin1String("id") || h == QLatin1String("no") || h == QLatin1String("key")
        || h == QLatin1String("pk") || h == QLatin1String("index"))
        return true;
    return h.endsWith(QLatin1String("_id"));
}

// Reads the first bare identifier from \a rest, stopping at whitespace or SQL
// punctuation. Returns empty if the next thing is an opening paren (subquery).
QString firstIdentifier(const QString &rest)
{
    QString tok;
    for (const QChar &ch : rest) {
        if (tok.isEmpty() && ch.isSpace())
            continue;               // skip leading whitespace
        if (ch.isSpace() || ch == QLatin1Char('(') || ch == QLatin1Char(')')
            || ch == QLatin1Char(',') || ch == QLatin1Char(';')
            || ch == QLatin1Char('['))
            break;
        if (ch == QLatin1Char('"') || ch == QLatin1Char('\'') || ch == QLatin1Char('`'))
            continue;               // strip surrounding quotes
        tok.append(ch);
    }
    return tok;
}

// snake_case / lower names → "Title Case Words" (drops any schema prefix).
QString prettifyName(const QString &raw)
{
    QString s = raw.trimmed();
    s = s.section(QLatin1Char('.'), -1);   // keep only the table part
    s.replace(QLatin1Char('_'), QLatin1Char(' '));
    QStringList words = s.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (QString &w : words) {
        if (w.isEmpty())
            continue;
        w = w.toLower();
        w[0] = w[0].toUpper();
    }
    return words.join(QLatin1Char(' '));
}

// Finds the main table a query reads from: the first FROM whose token is a
// real identifier (skips subquery parens), falling back to the first JOIN.
QString extractMainTable(const QString &sql)
{
    const QString up = sql.toUpper();
    auto candidateAt = [&](int kwIndex) -> QString {
        // Ensure the keyword is a whole word, not part of an identifier.
        const bool prevOk = kwIndex == 0 || !(up.at(kwIndex - 1).isLetterOrNumber()
                                              || up.at(kwIndex - 1) == QLatin1Char('_'));
        if (!prevOk)
            return QString();
        return firstIdentifier(sql.mid(kwIndex + 4));
    };
    int idx = 0;
    while ((idx = up.indexOf(QStringLiteral("FROM"), idx)) != -1) {
        const QString t = candidateAt(idx);
        if (!t.isEmpty())
            return prettifyName(t);
        idx += 4;
    }
    idx = 0;
    while ((idx = up.indexOf(QStringLiteral("JOIN"), idx)) != -1) {
        const QString t = candidateAt(idx);
        if (!t.isEmpty())
            return prettifyName(t);
        idx += 4;
    }
    return QString();
}

} // namespace

DashboardServer::DashboardServer(QObject *parent)
    : QObject(parent), m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &DashboardServer::onNewConnection);
}

DashboardServer::~DashboardServer()
{
    stop();
}

bool DashboardServer::start(quint16 preferredPort)
{
    if (m_server->isListening())
        return true;
    const QHostAddress local(QHostAddress::LocalHost);
    if (m_server->listen(local, preferredPort))
        return true;
    // Preferred port busy: fall back to any free ephemeral port.
    return m_server->listen(local, 0);
}

void DashboardServer::stop()
{
    if (m_server && m_server->isListening())
        m_server->close();
}

bool DashboardServer::isRunning() const
{
    return m_server->isListening();
}

quint16 DashboardServer::port() const
{
    return m_server->serverPort();
}

QString DashboardServer::baseUrl() const
{
    return QStringLiteral("http://127.0.0.1:%1/").arg(m_server->serverPort());
}

QString DashboardServer::generateTitle(const QString &sql)
{
    return extractMainTable(sql);
}

QString DashboardServer::addPanel(const QString &title, const QString &sql,
                                  const QStringList &columns, const QVector<QVector<QString>> &rows)
{
    Panel p;
    p.id = QStringLiteral("p%1").arg(++m_seq);

    // Auto-generate a readable title when none was supplied, then de-dupe.
    QString base = title.trimmed();
    if (base.isEmpty()) {
        base = generateTitle(sql);
        if (base.isEmpty())
            base = QStringLiteral("Panel %1").arg(m_seq);
    }
    QString finalTitle = base;
    for (int n = 2;; ++n) {
        bool dup = false;
        for (const Panel &ex : m_panels) {
            if (ex.title.compare(finalTitle, Qt::CaseInsensitive) == 0) {
                dup = true;
                break;
            }
        }
        if (!dup)
            break;
        finalTitle = QStringLiteral("%1 (%2)").arg(base).arg(n);
    }
    p.title = finalTitle;

    p.sql = sql;
    p.columns = columns;
    p.rows = rows;
    p.createdAt = QDateTime::currentDateTime();
    m_panels.append(p);
    emit panelsChanged();
    return p.id;
}

bool DashboardServer::removePanel(const QString &id)
{
    for (int i = 0; i < m_panels.size(); ++i) {
        if (m_panels.at(i).id == id) {
            m_panels.removeAt(i);
            emit panelsChanged();
            return true;
        }
    }
    return false;
}

bool DashboardServer::hasPanel(const QString &id) const
{
    for (const Panel &p : m_panels)
        if (p.id == id)
            return true;
    return false;
}

QString DashboardServer::panelTitle(const QString &id) const
{
    for (const Panel &p : m_panels)
        if (p.id == id)
            return p.title;
    return QString();
}

void DashboardServer::onNewConnection()
{
    while (QTcpSocket *s = m_server->nextPendingConnection())
        handleClient(s);
}

void DashboardServer::handleClient(QTcpSocket *socket)
{
    // Accumulate the request head, then answer once and close (Connection: close).
    auto buf = std::make_shared<QByteArray>();
    connect(socket, &QTcpSocket::readyRead, this, [this, socket, buf]() {
        buf->append(socket->readAll());
        if (!buf->contains("\r\n\r\n") && !buf->contains("\n\n"))
            return; // headers not complete yet

        const QString head = QString::fromUtf8(buf->left(buf->indexOf("\r\n")));
        // Expect: GET /path HTTP/1.1
        QString path = QStringLiteral("/");
        const QStringList parts = head.split(QLatin1Char(' '));
        if (parts.size() >= 2)
            path = parts.at(1);

        QString body;
        if (path.startsWith(QStringLiteral("/page/"))) {
            const QString id = path.mid(6).section(QLatin1Char('?'), 0, 0);
            body = renderPage(id);
        } else {
            body = renderIndex();
        }

        const QByteArray utf = body.toUtf8();
        QByteArray resp;
        resp += "HTTP/1.1 200 OK\r\n";
        resp += "Content-Type: text/html; charset=utf-8\r\n";
        resp += "Cache-Control: no-store\r\n";
        resp += QStringLiteral("Content-Length: %1\r\n").arg(utf.size()).toLatin1();
        resp += "Connection: close\r\n\r\n";
        resp += utf;
        socket->write(resp);
        socket->flush();
        socket->disconnectFromHost();
    });
    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
}

QString DashboardServer::escapeHtml(const QString &text)
{
    QString out = text;
    out.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    out.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    out.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    out.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    return out;
}

QString DashboardServer::renderTable(const Panel &p) const
{
    if (p.columns.isEmpty())
        return QStringLiteral("<p class='muted'>Bu panelde kolon bulunmuyor.</p>");

    QString html;
    html += QStringLiteral("<div class='tablewrap'><table><thead><tr>");
    for (const QString &c : p.columns)
        html += QStringLiteral("<th>%1</th>").arg(escapeHtml(c));
    html += QStringLiteral("</tr></thead><tbody>");

    const int limit = qMin(p.rows.size(), 1000);
    for (int r = 0; r < limit; ++r) {
        html += QStringLiteral("<tr>");
        for (int c = 0; c < p.columns.size(); ++c)
            html += QStringLiteral("<td>%1</td>").arg(escapeHtml(p.rows.at(r).value(c)));
        html += QStringLiteral("</tr>");
    }
    html += QStringLiteral("</tbody></table></div>");
    if (p.rows.size() > limit)
        html += QStringLiteral("<p class='muted'>… %1 satır (yalnızca ilk %2 gösterildi)</p>")
                    .arg(p.rows.size()).arg(limit);
    else
        html += QStringLiteral("<p class='muted'>%1 satır × %2 kolon</p>")
                    .arg(p.rows.size()).arg(p.columns.size());
    return html;
}

QString DashboardServer::renderChart(const Panel &p) const
{
    if (p.rows.size() < 2 || p.columns.size() < 2)
        return QString();

    auto colNumeric = [&](int c, double *sample) {
        int numeric = 0, nonEmpty = 0;
        double sum = 0.0;
        for (int r = 0; r < p.rows.size(); ++r) {
            const QString v = p.rows.at(r).value(c);
            if (v.trimmed().isEmpty())
                continue;
            ++nonEmpty;
            double d = 0.0;
            if (parseNumber(v, &d)) {
                ++numeric;
                sum += d;
            }
        }
        if (sample)
            *sample = sum;
        return nonEmpty > 0 && numeric * 2 >= nonEmpty;
    };

    // X = first text-ish column; Y = first numeric column that isn't an id.
    int x = -1, y = -1;
    for (int c = 0; c < p.columns.size(); ++c) {
        double s = 0.0;
        if (!colNumeric(c, &s)) {
            if (x < 0) x = c;
        } else if (y < 0 && !isIdentifierName(p.columns.at(c))) {
            y = c;
        }
    }
    if (x < 0) x = 0;
    if (y < 0 || x == y)
        return QString(); // nothing sensible to plot

    struct Bar { QString label; double value; };
    QVector<Bar> bars;
    double maxV = 0.0;
    const int cap = qMin(p.rows.size(), 24);
    for (int r = 0; r < cap; ++r) {
        double d = 0.0;
        parseNumber(p.rows.at(r).value(y), &d);
        bars.append({p.rows.at(r).value(x), d});
        maxV = qMax(maxV, d);
    }
    if (maxV <= 0.0)
        for (const Bar &b : bars)
            maxV = qMax(maxV, b.value);
    if (maxV <= 0.0)
        maxV = 1.0;

    QString html;
    html += QStringLiteral("<div class='chart'><div class='charttitle'>%1 → %2</div>")
                .arg(escapeHtml(p.columns.at(x)), escapeHtml(p.columns.at(y)));
    for (const Bar &b : bars) {
        const int pct = qMax(1, int((b.value / maxV) * 100.0));
        html += QStringLiteral("<div class='row'>"
                               "<span class='lbl'>%1</span>"
                               "<span class='track'><span class='bar' style='width:%2%'></span></span>"
                               "<span class='val'>%3</span></div>")
                    .arg(escapeHtml(b.label)).arg(pct).arg(QString::number(b.value, 'g', 6));
    }
    html += QStringLiteral("</div>");
    return html;
}

QString DashboardServer::renderIndex() const
{
    if (m_panels.isEmpty())
        return renderPage(QString());
    return renderPage(m_panels.first().id);
}

QString DashboardServer::renderPage(const QString &activeId) const
{
    const Panel *active = nullptr;
    for (const Panel &p : m_panels)
        if (p.id == activeId)
            active = &p;

    QString sidebar;
    if (m_panels.isEmpty()) {
        sidebar = QStringLiteral("<div class='empty'>Henüz panel yok.<br>"
                                 "Sonuçlardaki “📌 Panoya Ekle” kutusunu işaretleyerek "
                                 "bir sonucu buraya sabitleyin.</div>");
    } else {
        for (const Panel &p : m_panels)
            sidebar += QStringLiteral("<a class='item%1' href='/page/%2'>%3</a>")
                           .arg(p.id == activeId ? QStringLiteral(" active") : QString(),
                                p.id, escapeHtml(p.title));
    }

    QString main;
    if (active) {
        const QString chart = renderChart(*active);
        main += QStringLiteral("<div class='card'><h2>%1</h2>").arg(escapeHtml(active->title));
        if (!chart.isEmpty())
            main += chart;
        main += renderTable(*active);
        main += QStringLiteral("</div>");
    } else if (!m_panels.isEmpty()) {
        main = QStringLiteral("<div class='card'><h2>QuickDb Dashboard</h2>"
                              "<p class='muted'>Soldaki listeden bir panel seçin.</p></div>");
    } else {
        main = QStringLiteral("<div class='card'><h2>QuickDb Dashboard</h2>"
                              "<p class='muted'>Burada gösterilecek bir sonuç henüz sabitlenmedi.</p></div>");
    }

    return QStringLiteral(R"HTML(<!DOCTYPE html>
<html lang="tr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>QuickDb Dashboard</title>
<style>
:root{--green:#2e7d32;--green-d:#1b5e20;--bg:#f4f6f5;--line:#e0e4e2;}
*{box-sizing:border-box;}
body{margin:0;font-family:-apple-system,Segoe UI,Roboto,Helvetica,Arial,sans-serif;background:var(--bg);color:#222;}
header{background:var(--green);color:#fff;padding:14px 22px;font-weight:700;font-size:18px;letter-spacing:.3px;}
header small{font-weight:400;opacity:.85;font-size:12px;margin-left:10px;}
.layout{display:flex;min-height:calc(100vh - 52px);}
nav{width:260px;background:#fff;border-right:1px solid var(--line);padding:14px 10px;}
nav .item{display:block;padding:10px 12px;border-radius:8px;color:#1b5e20;text-decoration:none;font-weight:600;margin-bottom:4px;}
nav .item:hover{background:#eaf3ea;}
nav .item.active{background:var(--green);color:#fff;}
nav .empty{color:#777;font-size:13px;padding:8px;line-height:1.5;}
main{flex:1;padding:22px;}
.card{background:#fff;border:1px solid var(--line);border-radius:12px;padding:18px 20px;box-shadow:0 1px 3px rgba(0,0,0,.05);}
.card h2{margin:0 0 6px;color:var(--green-d);}
.sql{background:#f7f9f7;border:1px solid var(--line);border-radius:8px;padding:8px 10px;color:#33691e;
font-family:ui-monospace,Menlo,Consolas,monospace;font-size:12px;white-space:pre-wrap;overflow:auto;margin:8px 0 16px;}
.muted{color:#777;font-size:12px;}
.tablewrap{overflow:auto;max-height:70vh;border:1px solid var(--line);border-radius:8px;}
table{border-collapse:collapse;width:100%;font-size:13px;}
th{background:var(--green);color:#fff;text-align:left;padding:8px 10px;position:sticky;top:0;}
td{padding:7px 10px;border-top:1px solid var(--line);}
tbody tr:nth-child(even){background:#fafbfa;}
.chart{margin:6px 0 18px;}
.charttitle{font-weight:700;color:var(--green-d);margin-bottom:8px;font-size:14px;}
.chart .row{display:flex;align-items:center;font-size:12px;margin:3px 0;}
.chart .lbl{width:150px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;padding-right:8px;}
.chart .track{flex:1;background:#eef2ee;border-radius:4px;height:18px;}
.chart .bar{display:block;height:18px;background:linear-gradient(90deg,var(--green),#66bb6a);border-radius:4px;}
.chart .val{width:110px;text-align:right;padding-left:8px;color:#444;}
</style></head>
<body>
<header>QuickDb Dashboard<small>localhost web paneli</small></header>
<div class="layout"><nav>%1</nav><main>%2</main></div>
</body></html>)HTML")
        .arg(sidebar, main);
}

} // namespace core
