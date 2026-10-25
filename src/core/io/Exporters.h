#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace core {
namespace io {

/// Widget-free serialization of a rectangular result set (columns + stringified
/// rows) into common exchange formats. Every function returns the full document
/// as a QString so callers can write it to disk, preview it, or feed it to tests.
/// XLSX is intentionally out of scope (needs an external library).
struct ExportInput {
    QStringList columns;
    QVector<QVector<QString>> rows;
    QString tableName; // optional hint (used by SQL dump / XML root attr)
};

/// RFC-4180-ish CSV. Fields containing the delimiter, a double-quote or a newline
/// are quoted and inner quotes doubled. Header row always emitted.
QString toCsv(const ExportInput &in, QChar delimiter = QLatin1Char(','));

/// Array of objects, one per row, keyed by column name. Values are emitted as
/// JSON strings to guarantee an exact textual round-trip (type inference happens
/// on import, not here). Empty cells become JSON null.
QString toJson(const ExportInput &in);

/// <table name="..."> with a <row> per record and one child element per column.
QString toXml(const ExportInput &in);

/// A transaction of INSERT statements quoting identifiers/-values for SQLite.
/// NULL cells are emitted as the SQL NULL keyword.
QString toSqlDump(const ExportInput &in);

} // namespace io
} // namespace core
