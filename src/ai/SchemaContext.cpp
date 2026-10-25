#include "ai/SchemaContext.h"

#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSet>
#include <QStringList>

namespace ai {

namespace {

constexpr int kMaxSamples = 3;

QString brief(const QVariant &value)
{
    const QString s = value.toString();
    return s.size() > 24 ? s.left(21) + QStringLiteral("...") : s;
}

} // namespace

bool SchemaContext::looksNumeric(const QString &type)
{
    const QString t = type.toUpper();
    return t.contains(QStringLiteral("INT")) || t.contains(QStringLiteral("NUM"))
           || t.contains(QStringLiteral("DEC")) || t.contains(QStringLiteral("MONEY"))
           || t.contains(QStringLiteral("REAL")) || t.contains(QStringLiteral("FLOA"))
           || t.contains(QStringLiteral("DOUB"));
}

bool SchemaContext::looksDateish(const QString &name, const QString &type)
{
    const QString n = name.toLower();
    const QString t = type.toUpper();
    static const QSet<QString> hints = {
        QStringLiteral("date"), QStringLiteral("datetime"), QStringLiteral("time"),
        QStringLiteral("month"), QStringLiteral("ay"), QStringLiteral("tarih"),
        QStringLiteral("created"), QStringLiteral("updated"), QStringLiteral("gun"),
        QStringLiteral("yil"), QStringLiteral("year")};
    for (const QString &h : hints) {
        if (foldTr(n).contains(h))
            return true;
    }
    return t.contains(QStringLiteral("DATE")) || t.contains(QStringLiteral("TIME"));
}

QString foldTr(const QString &s)
{
    QString out = s.toLower();
    out.replace(QStringLiteral("ı"), QStringLiteral("i"));
    out.replace(QStringLiteral("İ"), QStringLiteral("i"));
    out.replace(QStringLiteral("ş"), QStringLiteral("s"));
    out.replace(QStringLiteral("ğ"), QStringLiteral("g"));
    out.replace(QStringLiteral("ü"), QStringLiteral("u"));
    out.replace(QStringLiteral("ö"), QStringLiteral("o"));
    out.replace(QStringLiteral("ç"), QStringLiteral("c"));
    return out;
}

const SchemaTable *SchemaContext::findTable(const QString &word) const
{
    const QString w = foldTr(word);
    if (w.isEmpty())
        return nullptr;
    for (const SchemaTable &t : tables) {
        const QString name = foldTr(t.name);
        // Exact or possessive/plural suffix match: "satislarin" ~ "satislar".
        if (w == name || w.startsWith(name) || name.startsWith(w.left(qMax(4, name.size() - 2))))
            return &t;
    }
    return nullptr;
}

const SchemaTable *SchemaContext::bestTableForQuestion(const QString &question) const
{
    const QString q = foldTr(question);
    // 1) Whole table name appearing inside the sentence.
    for (const SchemaTable &t : tables) {
        const QString name = foldTr(t.name);
        if (name.size() >= 3 && q.contains(name))
            return &t;
    }
    // 2) Token-by-token match (handles suffixes like "kullanicilarinda"
    //    against table "kullanicilar").
    const QStringList tokens = q.split(QRegularExpression(QStringLiteral("[^\\p{L}0-9_]+")),
                                       Qt::SkipEmptyParts);
    for (const QString &token : tokens) {
        if (const SchemaTable *t = findTable(token))
            return t;
    }
    // 3) No table name in the sentence: score tables by how well their column
    //    names / sample values overlap with the question tokens, then fall
    //    back to the first table so the offline engine still answers.
    const SchemaTable *best = nullptr;
    int bestScore = 0;
    for (const SchemaTable &t : tables) {
        int score = 0;
        for (const SchemaColumn &c : t.columns) {
            const QString cname = foldTr(c.name);
            if (cname == QLatin1String("id"))
                continue;
            if (q.contains(cname))
                score += 10;
            for (const QString &token : tokens) {
                if (token.size() >= 4 && (cname.contains(token) || token.contains(cname)))
                    score += 4;
            }
        }
        if (score > bestScore) {
            bestScore = score;
            best = &t;
        }
    }
    if (best)
        return best;
    return tables.isEmpty() ? nullptr : &tables.first();
}

const SchemaColumn *SchemaContext::numericColumn(const SchemaTable &table) const
{
    const SchemaColumn *fallback = nullptr;
    for (const SchemaColumn &c : table.columns) {
        if (!looksNumeric(c.type))
            continue;
        if (!c.primaryKey)
            return &c;
        fallback = fallback ? fallback : &c;
    }
    return fallback;
}

const SchemaColumn *SchemaContext::pickColumn(
    const SchemaTable &table,
    const std::function<int(const SchemaColumn &)> &scoreOf) const
{
    const SchemaColumn *best = nullptr;
    int bestScore = 0;
    for (const SchemaColumn &c : table.columns) {
        const int score = scoreOf(c);
        if (score > bestScore) {
            bestScore = score;
            best = &c;
        }
    }
    return best;
}

const SchemaColumn *SchemaContext::dateColumn(const SchemaTable &table) const
{
    for (const SchemaColumn &c : table.columns) {
        if (looksDateish(c.name, c.type))
            return &c;
    }
    return nullptr;
}

const SchemaColumn *SchemaContext::textColumn(const SchemaTable &table) const
{
    for (const SchemaColumn &c : table.columns) {
        if (!looksNumeric(c.type) && !looksDateish(c.name, c.type) && !c.primaryKey)
            return &c;
    }
    for (const SchemaColumn &c : table.columns) {
        if (!looksNumeric(c.type) && !looksDateish(c.name, c.type))
            return &c;
    }
    return nullptr;
}

QString SchemaContext::toPromptText() const
{
    QString out;
    for (const SchemaTable &t : tables) {
        QStringList cols;
        for (const SchemaColumn &c : t.columns) {
            QString line = QStringLiteral("  %1 %2").arg(c.name, c.type.isEmpty() ? QStringLiteral("TEXT") : c.type);
            if (c.primaryKey)
                line += QStringLiteral(" PRIMARY KEY");
            if (!c.samples.isEmpty())
                line += QStringLiteral("  -- ornek: %1").arg(c.samples.join(QStringLiteral(", ")));
            cols.append(line);
        }
        out += QStringLiteral("CREATE TABLE %1 (\n%2);\n")
                   .arg(t.name, cols.join(QStringLiteral(",\n")));
        for (const core::ForeignKeyInfo &fk : t.foreignKeys)
            out += QStringLiteral("-- FK: %1.%2 -> %3.%4\n")
                       .arg(t.name, fk.fromColumn, fk.toTable, fk.toColumn);
    }
    return out;
}

SchemaContext SchemaContext::fromDatabase(const core::DatabaseManager &db)
{
    SchemaContext ctx;
    if (!db.isConnected())
        return ctx;

    const QSqlDatabase handle = db.database();
    for (const QString &table : db.tableNames()) {
        SchemaTable t;
        t.name = table;
        t.foreignKeys = db.foreignKeysOf(table);
        const QVector<core::ColumnInfo> columns = db.columnsOf(table);
        for (const core::ColumnInfo &ci : columns) {
            SchemaColumn c;
            c.name = ci.name;
            c.type = ci.type;
            c.primaryKey = ci.primaryKey;
            // Up to 3 sample values so the LLM can see literal formats.
            QSqlQuery q(handle);
            if (q.exec(QStringLiteral("SELECT %1 FROM %2 WHERE %1 IS NOT NULL LIMIT %3;")
                           .arg(core::DatabaseManager::quoteIdentifier(ci.name),
                                core::DatabaseManager::quoteIdentifier(table),
                                QString::number(kMaxSamples)))) {
                while (q.next())
                    c.samples << brief(q.value(0));
            }
            t.columns.append(c);
        }
        ctx.tables.append(t);
    }
    return ctx;
}

} // namespace ai
