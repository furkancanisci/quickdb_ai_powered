#include "core/io/Importers.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace core {
namespace io {

namespace {

QString readAll(const QString &path, bool *okOut, QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (err)
            *err = QStringLiteral("Cannot open file: %1").arg(f.errorString());
        if (okOut)
            *okOut = false;
        return QString();
    }
    const QByteArray raw = f.readAll();
    f.close();
    if (okOut)
        *okOut = true;
    return QString::fromUtf8(raw);
}

/// Splits one CSV line honoring RFC-4180 quoting (a field may span newlines is
/// NOT handled here; the caller feeds logical lines). Returns false when a quote
/// is left unterminated at end-of-line.
QStringList splitCsvLine(const QString &line, QChar delim, bool *wellFormed)
{
    QStringList out;
    QString cur;
    bool inQuotes = false;
    bool wf = true;
    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (inQuotes) {
            if (ch == QLatin1Char('"')) {
                if (i + 1 < line.size() && line.at(i + 1) == QLatin1Char('"')) {
                    cur += QLatin1Char('"');
                    ++i; // skip escaped quote
                } else {
                    inQuotes = false;
                }
            } else {
                cur += ch;
            }
        } else {
            if (ch == QLatin1Char('"')) {
                inQuotes = true;
            } else if (ch == delim) {
                out << cur;
                cur.clear();
            } else {
                cur += ch;
            }
        }
    }
    if (inQuotes)
        wf = false;
    out << cur;
    if (wellFormed)
        *wellFormed = wf;
    return out;
}

QString sqlLiteral(const QString &value)
{
    QString v = value;
    v.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QLatin1Char('\'') + v + QLatin1Char('\'');
}

QString quotedId(const QString &id)
{
    QString q = id;
    q.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(q);
}

} // namespace

ImportedTable parseCsv(const QString &path, QChar delimiter, bool hasHeader)
{
    ImportedTable t;
    QString err;
    bool ok = false;
    const QString content = readAll(path, &ok, &err);
    if (!ok) {
        t.error = err;
        return t;
    }

    // Split into physical lines, then regroup for embedded newlines inside
    // quoted fields.
    QString normalized = content;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QChar(QLatin1Char('\r')), QChar(QLatin1Char('\n')));
    const QStringList physical = normalized.split(QChar(QLatin1Char('\n')));
    QVector<QStringList> records;
    QString pending;
    bool havePending = false;
    for (const QString &ln : physical) {
        if (!havePending) {
            if (ln.isEmpty())
                continue; // skip blank lines outside a record
            pending = ln;
            havePending = true;
        } else {
            pending += QLatin1Char('\n') + ln;
        }
        bool wf = true;
        splitCsvLine(pending, delimiter, &wf);
        if (wf) {
            records << splitCsvLine(pending, delimiter, nullptr);
            pending.clear();
            havePending = false;
        }
    }
    if (havePending) // unterminated quote at EOF; emit best-effort last record
        records << splitCsvLine(pending, delimiter, nullptr);

    if (records.isEmpty()) {
        t.error = QStringLiteral("Empty CSV file.");
        return t;
    }

    int start = 0;
    if (hasHeader) {
        t.columns = records.first();
        start = 1;
    } else {
        const int n = records.first().size();
        for (int i = 0; i < n; ++i)
            t.columns << QStringLiteral("column_%1").arg(i + 1);
    }

    for (int r = start; r < records.size(); ++r) {
        QVector<QString> row;
        for (int c = 0; c < t.columns.size(); ++c)
            row << (c < records[r].size() ? records[r][c] : QString());
        t.rows << row;
    }
    t.ok = true;
    return t;
}

ImportedTable parseJson(const QString &path)
{
    ImportedTable t;
    QString err;
    bool ok = false;
    const QString content = readAll(path, &ok, &err);
    if (!ok) {
        t.error = err;
        return t;
    }

    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(content.toUtf8(), &perr);
    if (perr.error != QJsonParseError::NoError) {
        t.error = QStringLiteral("JSON parse error: %1").arg(perr.errorString());
        return t;
    }

    QJsonArray arr;
    if (doc.isArray()) {
        arr = doc.array();
    } else if (doc.isObject()) {
        // Pick the first member that is an array of objects.
        const QJsonObject obj = doc.object();
        for (const QJsonValue &v : obj) {
            if (v.isArray()) {
                arr = v.toArray();
                break;
            }
        }
    } else {
        t.error = QStringLiteral("JSON is neither an array nor an object.");
        return t;
    }

    // Column union in first-seen order.
    QStringList cols;
    QVector<QJsonObject> objects;
    for (const QJsonValue &v : arr) {
        if (!v.isObject())
            continue;
        const QJsonObject o = v.toObject();
        for (const QString &k : o.keys())
            if (!cols.contains(k))
                cols << k;
        objects << o;
    }
    t.columns = cols;

    for (const QJsonObject &o : objects) {
        QVector<QString> row;
        for (const QString &c : cols) {
            const QJsonValue v = o.value(c);
            QString s;
            switch (v.type()) {
            case QJsonValue::Bool:   s = v.toBool() ? QStringLiteral("true") : QStringLiteral("false"); break;
            case QJsonValue::Double: {
                const double d = v.toDouble();
                if (d == qRound(d))
                    s = QString::number(static_cast<qint64>(d));
                else
                    s = QString::number(d);
                break;
            }
            case QJsonValue::String: s = v.toString(); break;
            default:                 s = QString(); break; // Null/Undefined/Object/Array
            }
            row << s;
        }
        t.rows << row;
    }
    t.ok = true;
    return t;
}

QString inferColumnType(const ImportedTable &data, int col)
{
    bool any = false;
    bool allInt = true;
    bool allReal = true;
    for (const QVector<QString> &row : data.rows) {
        const QString v = col < row.size() ? row[col] : QString();
        if (v.trimmed().isEmpty())
            continue; // NULLs don't disqualify a numeric type
        any = true;
        bool isInt = false;
        const qlonglong ll = v.toLongLong(&isInt);
        Q_UNUSED(ll);
        bool isReal = false;
        v.toDouble(&isReal);
        if (!isInt)
            allInt = false;
        if (!isReal)
            allReal = false;
    }
    if (!any)
        return QStringLiteral("TEXT");
    if (allInt)
        return QStringLiteral("INTEGER");
    if (allReal)
        return QStringLiteral("REAL");
    return QStringLiteral("TEXT");
}

QString buildCreateTable(const QString &table, const ImportedTable &data)
{
    QStringList defs;
    for (int c = 0; c < data.columns.size(); ++c)
        defs << QStringLiteral("%1 %2").arg(quotedId(data.columns[c]), inferColumnType(data, c));
    return QStringLiteral("CREATE TABLE %1 (%2);")
        .arg(quotedId(table), defs.join(QStringLiteral(", ")));
}

QString buildInserts(const QString &table, const ImportedTable &data)
{
    QStringList colList;
    for (const QString &c : data.columns)
        colList << quotedId(c);
    const QString cols = colList.join(QStringLiteral(", "));
    QStringList lines;
    for (const QVector<QString> &row : data.rows) {
        QStringList vals;
        for (int c = 0; c < data.columns.size(); ++c) {
            const QString v = c < row.size() ? row[c] : QString();
            vals << (v.isEmpty() ? QStringLiteral("NULL") : sqlLiteral(v));
        }
        lines << QStringLiteral("INSERT INTO %1 (%2) VALUES (%3);")
                     .arg(quotedId(table), cols, vals.join(QStringLiteral(", ")));
    }
    return lines.join(QLatin1Char('\n'));
}

} // namespace io
} // namespace core
