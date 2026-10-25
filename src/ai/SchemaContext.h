#pragma once

#include <QVector>

#include <functional>

#include "core/DatabaseManager.h"

namespace ai {

/// One column entry inside the schema prompt: name : TYPE plus sample values.
struct SchemaColumn {
    QString name;
    QString type;
    bool primaryKey = false;
    QStringList samples; // up to 3 short, stringified values
};

/// One table entry: columns + foreign-key relations.
struct SchemaTable {
    QString name;
    QVector<SchemaColumn> columns;
    QVector<core::ForeignKeyInfo> foreignKeys;
};

/// Plain-data snapshot of the live schema, consumed by the LLM prompt,
/// the rule engine and tests alike. Deliberately DB-session independent
/// so it can be built by hand in unit tests.
struct SchemaContext {
    QVector<SchemaTable> tables;

    /// Case-insensitive table lookup (also strips common TR/EN suffixes like
    /// "tablolarinda", "satislarin" by prefix matching against table names).
    const SchemaTable *findTable(const QString &word) const;
    /// Table whose name appears as a substring of the question, else nullptr.
    const SchemaTable *bestTableForQuestion(const QString &question) const;
    /// First column of the table whose type looks numeric (not PK preferred).
    const SchemaColumn *numericColumn(const SchemaTable &table) const;
    /// First column that looks like a date/month label.
    const SchemaColumn *dateColumn(const SchemaTable &table) const;
    /// First non-numeric text column (good x-axis/category candidate).
    const SchemaColumn *textColumn(const SchemaTable &table) const;

    /// Score-driven column pick: scores every column with scoreOf() and
    /// returns the best one (nullptr when the table has no columns).
    /// Used by the rule engine to match question words ("tutar", "ciro")
    /// against column names in any language.
    const SchemaColumn *pickColumn(
        const SchemaTable &table,
        const std::function<int(const SchemaColumn &)> &scoreOf) const;

    /// Renders the schema like "CREATE TABLE ..." style for the LLM prompt,
    /// including sample values so the model can guess literal formats.
    QString toPromptText() const;

    /// Reads tables, columns, FKs and sample values from an open manager.
    static SchemaContext fromDatabase(const core::DatabaseManager &db);

    /// Shared type/name heuristics (also used by the rule engine).
    static bool looksNumeric(const QString &type);
    static bool looksDateish(const QString &name, const QString &type);
};

/// Lowercase with Turkish characters folded to ASCII so substring/regex
/// matching works across TR/EN wording and ASCII identifiers.
QString foldTr(const QString &s);

} // namespace ai
