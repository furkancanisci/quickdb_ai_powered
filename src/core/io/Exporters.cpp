#include "core/io/Exporters.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QXmlStreamWriter>

namespace core {
namespace io {

namespace {

/// Wraps a single CSV field, quoting only when necessary.
QString csvField(const QString &value, QChar delim)
{
    QString v = value;
    const bool needsQuotes = v.contains(delim) || v.contains(QLatin1Char('"'))
                             || v.contains(QLatin1Char('\n')) || v.contains(QLatin1Char('\r'));
    if (needsQuotes) {
        v.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QLatin1Char('"') + v + QLatin1Char('"');
    }
    return v;
}

/// Escapes a string literal for SQLite (doubles single quotes).
QString sqlLiteral(const QString &value)
{
    QString v = value;
    v.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QLatin1Char('\'') + v + QLatin1Char('\'');
}

} // namespace

QString toCsv(const ExportInput &in, QChar delimiter)
{
    QStringList lines;
    QStringList header;
    for (const QString &c : in.columns)
        header << csvField(c, delimiter);
    lines << header.join(delimiter);
    for (const QVector<QString> &row : in.rows) {
        QStringList cells;
        for (int i = 0; i < in.columns.size(); ++i)
            cells << csvField(i < row.size() ? row[i] : QString(), delimiter);
        lines << cells.join(delimiter);
    }
    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

QString toJson(const ExportInput &in)
{
    QJsonArray arr;
    for (const QVector<QString> &row : in.rows) {
        QJsonObject obj;
        for (int i = 0; i < in.columns.size(); ++i) {
            const QString v = i < row.size() ? row[i] : QString();
            obj.insert(in.columns[i], v.isEmpty() ? QJsonValue(QJsonValue::Null)
                                                  : QJsonValue(v));
        }
        arr.append(obj);
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

QString toXml(const ExportInput &in)
{
    QString out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(true);
    w.writeStartDocument();
    w.writeStartElement(QStringLiteral("table"));
    if (!in.tableName.isEmpty())
        w.writeAttribute(QStringLiteral("name"), in.tableName);
    for (const QVector<QString> &row : in.rows) {
        w.writeStartElement(QStringLiteral("row"));
        for (int i = 0; i < in.columns.size(); ++i) {
            w.writeTextElement(in.columns[i], i < row.size() ? row[i] : QString());
        }
        w.writeEndElement(); // row
    }
    w.writeEndElement(); // table
    w.writeEndDocument();
    return out;
}

QString toSqlDump(const ExportInput &in)
{
    const QString table = in.tableName.isEmpty() ? QStringLiteral("exported_table") : in.tableName;
    QStringList colList;
    for (const QString &c : in.columns) {
        QString q = c;
        q.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        colList << QStringLiteral("\"%1\"").arg(q);
    }
    const QString cols = colList.join(QStringLiteral(", "));
    QStringList lines;
    lines << QStringLiteral("-- QuickDb SQL dump: %1 (%2 rows)")
                 .arg(table).arg(in.rows.size());
    lines << QStringLiteral("BEGIN TRANSACTION;");
    for (const QVector<QString> &row : in.rows) {
        QStringList vals;
        for (int i = 0; i < in.columns.size(); ++i) {
            const QString v = i < row.size() ? row[i] : QString();
            vals << (v.isEmpty() ? QStringLiteral("NULL") : sqlLiteral(v));
        }
        lines << QStringLiteral("INSERT INTO \"%1\" (%2) VALUES (%3);")
                     .arg(table, cols, vals.join(QStringLiteral(", ")));
    }
    lines << QStringLiteral("COMMIT;");
    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

} // namespace io
} // namespace core
