#pragma once

#include <QStringList>

namespace ui {

/// Shared SQL vocabulary used by both the syntax highlighter and the
/// formatter. Lower-case entries; match case-insensitively.
inline QStringList sqlKeywordList()
{
    return {
        QStringLiteral("select"), QStringLiteral("from"), QStringLiteral("where"),
        QStringLiteral("insert"), QStringLiteral("into"), QStringLiteral("values"),
        QStringLiteral("update"), QStringLiteral("set"), QStringLiteral("delete"),
        QStringLiteral("create"), QStringLiteral("table"), QStringLiteral("view"),
        QStringLiteral("index"), QStringLiteral("trigger"), QStringLiteral("drop"),
        QStringLiteral("alter"), QStringLiteral("add"), QStringLiteral("column"),
        QStringLiteral("and"), QStringLiteral("or"), QStringLiteral("not"), QStringLiteral("null"),
        QStringLiteral("primary"), QStringLiteral("key"), QStringLiteral("foreign"),
        QStringLiteral("references"), QStringLiteral("unique"), QStringLiteral("default"),
        QStringLiteral("join"), QStringLiteral("inner"), QStringLiteral("left"),
        QStringLiteral("right"), QStringLiteral("outer"), QStringLiteral("on"),
        QStringLiteral("cross"), QStringLiteral("full"), QStringLiteral("natural"),
        QStringLiteral("group"), QStringLiteral("by"), QStringLiteral("order"),
        QStringLiteral("having"), QStringLiteral("limit"), QStringLiteral("offset"),
        QStringLiteral("distinct"), QStringLiteral("as"), QStringLiteral("asc"),
        QStringLiteral("desc"), QStringLiteral("begin"), QStringLiteral("commit"),
        QStringLiteral("rollback"), QStringLiteral("transaction"),
        QStringLiteral("is"), QStringLiteral("between"),
        QStringLiteral("like"), QStringLiteral("ilike"), QStringLiteral("glob"),
        QStringLiteral("union"), QStringLiteral("except"), QStringLiteral("intersect"),
        QStringLiteral("all"), QStringLiteral("pragma"), QStringLiteral("explain"),
        QStringLiteral("vacuum"), QStringLiteral("attach"), QStringLiteral("detach"),
        QStringLiteral("grant"), QStringLiteral("revoke"),
        QStringLiteral("returning"), QStringLiteral("conflict"), QStringLiteral("replace")
    };
}

/// CTE / window / conditional vocabulary rendered with its own accent color.
inline QStringList sqlSpecialList()
{
    return {
        QStringLiteral("with"), QStringLiteral("recursive"), QStringLiteral("over"),
        QStringLiteral("partition"), QStringLiteral("rows"), QStringLiteral("range"),
        QStringLiteral("preceding"), QStringLiteral("following"), QStringLiteral("current"),
        QStringLiteral("case"), QStringLiteral("when"), QStringLiteral("then"),
        QStringLiteral("else"), QStringLiteral("end"), QStringLiteral("exists"),
        QStringLiteral("in"), QStringLiteral("cast"), QStringLiteral("filter"),
        QStringLiteral("within"), QStringLiteral("nulls"), QStringLiteral("first"),
        QStringLiteral("last"), QStringLiteral("using"), QStringLiteral("cascade"),
        QStringLiteral("restrict"), QStringLiteral("if"), QStringLiteral("true"),
        QStringLiteral("false"), QStringLiteral("fetch"), QStringLiteral("next")
    };
}

/// Data type names (SQLite + PostgreSQL superset).
inline QStringList sqlTypeList()
{
    return {
        QStringLiteral("integer"), QStringLiteral("int"), QStringLiteral("smallint"),
        QStringLiteral("bigint"), QStringLiteral("serial"), QStringLiteral("bigserial"),
        QStringLiteral("numeric"), QStringLiteral("decimal"), QStringLiteral("real"),
        QStringLiteral("double"), QStringLiteral("float"), QStringLiteral("money"),
        QStringLiteral("text"), QStringLiteral("char"), QStringLiteral("varchar"),
        QStringLiteral("character"), QStringLiteral("nvarchar"), QStringLiteral("blob"),
        QStringLiteral("clob"), QStringLiteral("boolean"), QStringLiteral("bool"),
        QStringLiteral("date"), QStringLiteral("time"), QStringLiteral("timestamp"),
        QStringLiteral("datetime"), QStringLiteral("interval"), QStringLiteral("uuid"),
        QStringLiteral("json"), QStringLiteral("jsonb"), QStringLiteral("xml"),
        QStringLiteral("array")
    };
}

} // namespace ui
