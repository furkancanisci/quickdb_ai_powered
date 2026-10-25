#pragma once

#include <QString>

#include "ai/SchemaContext.h"

namespace ai {

/// Offline, deterministic natural-language -> SQL engine (no LLM required).
/// Maps TR/EN question keywords plus live schema introspection onto a small
/// set of query shapes: COUNT, SUM-grouped, trend-by-date, Top-N, LIKE lookup
/// and plain SELECT. The LLM path falls back here when Ollama is unreachable.
class RuleBasedNlu {
public:
    enum class Intent { Count, Sum, Trend, TopN, Lookup, Select };

    struct Plan {
        bool ok = false;
        Intent intent = Intent::Select;
        QString sql;         // ready-to-run, read-only statement
        QString tableName;   // resolved table (empty => clarification needed)
        QString explanation; // short human-readable description (TR)
    };

    static Plan run(const QString &question, const SchemaContext &schema);

    /// Returns a friendly Turkish chat reply when the input is purely
    /// conversational (greeting / thanks / how-are-you / who-are-you / bye /
    /// help) with no data intent, so the assistant answers in prose instead of
    /// turning "merhaba" into SQL. Returns an empty string for real questions.
    static QString conversationalReply(const QString &question);

    /// True when the statement is read-only (SELECT/WITH/PRAGMA/EXPLAIN and
    /// no data-modifying keyword). Shared with the LLM path as a safety gate.
    static bool isReadOnlySql(const QString &sql);

    /// Pulls the first SQL statement out of free-form LLM text (```code
    /// fences, "SQL:" prefixes, trailing prose). Returns the candidate or an
    /// empty string.
    static QString extractSqlFromText(const QString &text);
};

} // namespace ai
