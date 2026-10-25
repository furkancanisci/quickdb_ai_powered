#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace core {
namespace io {

/// Result of parsing a data file into a rectangular table plus a schema
/// suggestion. `columns`/`rows` mirror the export side so the two round-trip.
struct ImportedTable {
    bool ok = false;
    QString error;
    QStringList columns;
    QVector<QVector<QString>> rows;

    int columnCount() const { return columns.size(); }
    int rowCount() const { return rows.size(); }
};

/// Parses a delimited text file. Handles quoted fields (double quotes, "" to
/// escape). `hasHeader` controls whether the first line names the columns.
ImportedTable parseCsv(const QString &path, QChar delimiter = QLatin1Char(','),
                       bool hasHeader = true);

/// Parses a JSON file that is either an array of objects or an object with a
/// single array-of-objects member. Union of keys (first-seen order) becomes the
/// column list; each object's values are stringified.
ImportedTable parseJson(const QString &path);

/// Infers a SQLite column type per column: INTEGER, REAL or TEXT (empty column
/// -> TEXT). Used when suggesting a CREATE TABLE for an imported file.
QString inferColumnType(const ImportedTable &data, int col);

/// Builds a CREATE TABLE statement for the given table using inferred types.
QString buildCreateTable(const QString &table, const ImportedTable &data);

/// One INSERT statement per row (SQLite-safe quoting; empty cell -> NULL).
QString buildInserts(const QString &table, const ImportedTable &data);

} // namespace io
} // namespace core
