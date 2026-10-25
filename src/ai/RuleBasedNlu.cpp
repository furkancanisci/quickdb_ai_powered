#include "ai/RuleBasedNlu.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

#include <initializer_list>

namespace ai {

namespace {

/// First integer word/digit in the folded text, or -1.
int findNumber(const QString &folded)
{
    static const QRegularExpression digits(QStringLiteral("\\b(\\d{1,3})\\b"));
    QRegularExpressionMatch m = digits.match(folded);
    if (m.hasMatch())
        return m.captured(1).toInt();

    // Turkish/English number words up to twelve.
    static const QHash<QString, int> words = {
        {QStringLiteral("bir"), 1}, {QStringLiteral("iki"), 2},
        {QStringLiteral("uc"), 3},  {QStringLiteral("dort"), 4},
        {QStringLiteral("bes"), 5}, {QStringLiteral("alti"), 6},
        {QStringLiteral("yedi"), 7}, {QStringLiteral("sekiz"), 8},
        {QStringLiteral("dokuz"), 9}, {QStringLiteral("on bir"), 11},
        {QStringLiteral("on iki"), 12}, {QStringLiteral("one"), 1},
        {QStringLiteral("two"), 2}, {QStringLiteral("three"), 3},
        {QStringLiteral("four"), 4}, {QStringLiteral("five"), 5},
        {QStringLiteral("six"), 6}, {QStringLiteral("seven"), 7},
        {QStringLiteral("eight"), 8}, {QStringLiteral("nine"), 9},
        {QStringLiteral("ten"), 10}};
    for (auto it = words.cbegin(); it != words.cend(); ++it) {
        if (folded.contains(QStringLiteral(" ") + it.key() + QStringLiteral(" ")))
            return it.value();
    }
    return -1;
}

bool containsAny(const QString &text, const QStringList &needles)
{
    for (const QString &n : needles) {
        if (text.contains(n))
            return true;
    }
    return false;
}

/// Loose text match: both sides folded to lowercase ASCII so that Turkish
/// wording ("en yüksek") matches regardless of diacritics.
QString foldLoose(const QString &s) { return foldTr(s); }

QString q(const QString &identifier)
{
    return core::DatabaseManager::quoteIdentifier(identifier);
}

/// Does any question token plausibly belong to this column (name or sample)?
int columnScore(const SchemaColumn &c, const QString &foldedQuestion)
{
    int score = 0;
    const QString name = foldTr(c.name);
    if (name == QLatin1String("id"))
        score -= 4; // never aggregate/label by surrogate keys
    if (foldedQuestion.contains(name))
        score += 10;
    for (const QString &tok : foldedQuestion.split(QRegularExpression(QStringLiteral("[^\\p{L}0-9_]+")),
                                                   Qt::SkipEmptyParts)) {
        if (tok.size() >= 4 && (name.contains(tok) || tok.contains(name)))
            score += 5;
    }
    return score;
}

const SchemaColumn *bestColumn(const SchemaContext &ctx, const SchemaTable &table,
                               const QString &foldedQuestion,
                               bool (*accepts)(const SchemaColumn &),
                               const SchemaColumn *fallback)
{
    const SchemaColumn *best = ctx.pickColumn(
        table, [&](const SchemaColumn &c) -> int {
            if (!accepts(c))
                return 0;
            return columnScore(c, foldedQuestion) + 1; // type-fit bonus
        });
    return best ? best : fallback;
}

bool acceptsNumeric(const SchemaColumn &c) { return SchemaContext::looksNumeric(c.type); }
bool acceptsText(const SchemaColumn &c)
{
    return !SchemaContext::looksNumeric(c.type) && !c.primaryKey;
}

/// Literal quoted inside quotes/double-quotes in the question, if any.
QString quotedLiteral(const QString &question)
{
    static const QRegularExpression re(QStringLiteral("[\"'“”]([^\"'“”]{1,40})[\"'“”]"));
    const QRegularExpressionMatch m = re.match(question);
    return m.hasMatch() ? m.captured(1).trimmed() : QString();
}

/// Does a question token match one of the column's sample values?
bool sampleHit(const SchemaColumn &c, const QStringList &tokens)
{
    for (const QString &tok : tokens) {
        if (tok.size() < 3)
            continue;
        for (const QString &sample : c.samples) {
            if (foldTr(sample).startsWith(tok) || tok.startsWith(foldTr(sample)))
                return true;
        }
    }
    return false;
}

} // namespace

bool RuleBasedNlu::isReadOnlySql(const QString &sql)
{
    const QString trimmed = sql.trimmed().toLower();
    if (trimmed.isEmpty())
        return false;
    static const QSet<QString> allowedHeads = {
        QStringLiteral("select"), QStringLiteral("with"),
        QStringLiteral("pragma"), QStringLiteral("explain")};
    const QString head = trimmed.left(trimmed.indexOf(QRegularExpression(QStringLiteral("[\\s(]"))));
    if (!allowedHeads.contains(head))
        return false;
    // Reject stacked statements and hidden writes ("SELECT...;DROP...").
    static const QStringList forbidden = {
        QStringLiteral("insert"), QStringLiteral("update"), QStringLiteral("delete"),
        QStringLiteral("drop"),   QStringLiteral("alter"),  QStringLiteral("create"),
        QStringLiteral("attach"), QStringLiteral("detach"), QStringLiteral("vacuum")};
    QString noStrings = trimmed;
    noStrings.remove(QRegularExpression(QStringLiteral("'[^']*'")));
    for (const QString &f : forbidden) {
        if (QRegularExpression(QStringLiteral("\\b") + f + QStringLiteral("\\b")).match(noStrings).hasMatch())
            return false;
    }
    return true;
}

QString RuleBasedNlu::extractSqlFromText(const QString &text)
{
    QString t = text.trimmed();
    // 1) ```sql ... ``` fence.
    static const QRegularExpression fence(
        QStringLiteral("```(?:sql)?\\s*(.+?)\\s*```"),
        QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch m = fence.match(t);
    if (m.hasMatch())
        t = m.captured(1).trimmed();

    // 2) "SQL:" style prefix.
    static const QRegularExpression prefix(
        QStringLiteral("^(?:sql|sorgu|query)\\s*[:>=]\\s*"),
        QRegularExpression::CaseInsensitiveOption);
    t.remove(prefix);

    // 3) A WITH ... query must be kept WHOLE: the SELECT-span pull below
    //    would start at the first inner SELECT and silently drop the CTE
    //    header, producing an unexecutable fragment.
    if (t.startsWith(QStringLiteral("with"), Qt::CaseInsensitive))
        return t;

    // 4) First SELECT ... ; span.
    static const QRegularExpression select(
        QStringLiteral("(?i)\\b(select\\b.+?)(?:;|$)"), QRegularExpression::DotMatchesEverythingOption);
    m = select.match(t);
    if (m.hasMatch())
        return m.captured(1).trimmed();
    if (t.toLower().startsWith(QStringLiteral("select")))
        return t;
    return QString();
}

RuleBasedNlu::Plan RuleBasedNlu::run(const QString &question, const SchemaContext &schema)
{
    Plan plan;
    const QString folded = foldLoose(question);

    const SchemaTable *table = schema.bestTableForQuestion(question);
    if (!table && schema.tables.size() == 1)
        table = &schema.tables.first();
    if (!table) {
        plan.explanation = QStringLiteral(
            "Hangi tabloyu kastettigini bulamadin. Tablo adini belirterek sorun "
            "(mevcut tablolar: %1).")
            .arg(QStringList(schema.tables.isEmpty() ? QStringList{QStringLiteral("-")}
                    : [&]{ QStringList names; for (const SchemaTable &t : schema.tables) names << t.name; return names; }())
                 .join(QStringLiteral(", ")));
        return plan;
    }
    plan.tableName = table->name;

    // ---- Intent detection (priority order) ------------------------------
    const bool trendish = containsAny(folded, {
        QStringLiteral("trend"), QStringLiteral("aylik"), QStringLiteral("ay ay"),
        QStringLiteral("aylara"), QStringLiteral("zamanla"), QStringLiteral("monthly"),
        QStringLiteral("per month"), QStringLiteral("timeline")});
    const bool countish = containsAny(folded, {
        QStringLiteral("kac tane"), QStringLiteral("kac satir"), QStringLiteral("kac kayit"),
        QStringLiteral("kac kisi"), QStringLiteral("kac "), QStringLiteral("adet"),
        QStringLiteral("sayisi"), QStringLiteral("say"), QStringLiteral("how many"),
        QStringLiteral("count"), QStringLiteral("total number")});
    const bool sumish = containsAny(folded, {
        QStringLiteral("toplam"), QStringLiteral("sum"), QStringLiteral("ciro"),
        QStringLiteral("hasilat"), QStringLiteral("gelir"), QStringLiteral("overall"),
        QStringLiteral("revenue")});
    const bool topish = containsAny(folded, {
        QStringLiteral("en yuk"), QStringLiteral("en buyu"), QStringLiteral("en iyi"),
        QStringLiteral("ilk "), QStringLiteral("top "), QStringLiteral("highest"),
        QStringLiteral("largest"), QStringLiteral("liste"), QStringLiteral("sirala")});

    const QStringList tokens = folded.split(QRegularExpression(QStringLiteral("[^\\p{L}0-9_]+")),
                                            Qt::SkipEmptyParts);

    // Date/month column shared by trend + sum-group paths.
    const SchemaColumn *dateCol = schema.dateColumn(*table);
    const SchemaColumn *numCol = bestColumn(schema, *table, folded, acceptsNumeric,
                                            schema.numericColumn(*table));
    const SchemaColumn *txtCol = bestColumn(schema, *table, folded, acceptsText,
                                            schema.textColumn(*table));

    if (countish) {
        plan.intent = Intent::Count;
        plan.sql = QStringLiteral("SELECT COUNT(*) AS say FROM %1;").arg(q(table->name));
        plan.explanation = QStringLiteral("'%1' tablosundaki satır sayısı.").arg(table->name);
        plan.ok = true;
        return plan;
    }

    if (trendish) {
        plan.intent = Intent::Trend;
        if (!dateCol) {
            plan.explanation = QStringLiteral(
                "'%1' tablosunda tarih/ay kolonu bulamadim; trend icin bir tarih "
                "kolonu gerekir.").arg(table->name);
            return plan;
        }
        // Decide how to group the time axis. strftime() rollup is only valid
        // for full dates (YYYY-MM-DD) or numeric timestamps; a column that
        // already stores 'YYYY-MM' text must be grouped raw, otherwise
        // strftime() returns NULL and every row collapses into one bucket.
        const bool numericDate = SchemaContext::looksNumeric(dateCol->type);
        const QString sample0 = dateCol->samples.isEmpty() ? QString() : dateCol->samples.first();
        const bool fullDate = QRegularExpression(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}"))
                                  .match(sample0).hasMatch();
        const bool monthText = QRegularExpression(QStringLiteral("^\\d{4}-\\d{2}$"))
                                   .match(sample0).hasMatch();
        const bool rollup = numericDate || fullDate; // needs a real date for strftime
        const QString groupExpr = rollup
            ? QStringLiteral("strftime('%Y-%m', %1)").arg(q(dateCol->name))
            : q(dateCol->name);
        QString where;
        const int months = [&folded]() -> int {
            static const QRegularExpression re(QStringLiteral("(\\d{1,2})\\s+(ay|month)"));
            const QRegularExpressionMatch m = re.match(folded);
            if (m.hasMatch())
                return m.captured(1).toInt();
            return findNumber(folded);
        }();
        if (months > 0 && rollup) {
            // ISO date column: filter the recent window relative to today.
            where = QStringLiteral(" WHERE %1 >= date('now', '-%2 months')")
                        .arg(q(dateCol->name)).arg(months);
        } else if (months > 0 && monthText) {
            // 'YYYY-MM' text: keep the recent window by string prefix order.
            where = QStringLiteral(" WHERE %1 >= strftime('%Y-%m', 'now', '-%2 months')")
                        .arg(q(dateCol->name)).arg(months);
        }
        const QString valueExpr = numCol
            ? QStringLiteral("SUM(%1) AS toplam").arg(q(numCol->name))
            : QStringLiteral("COUNT(*) AS toplam");
        plan.sql = QStringLiteral("SELECT %1 AS ay, %2 FROM %3%4 GROUP BY %1 ORDER BY %1;")
                       .arg(groupExpr, valueExpr, q(table->name), where);
        plan.explanation = QStringLiteral("'%1' icin aylık %2 trendi.")
                               .arg(table->name, numCol ? numCol->name : QStringLiteral("satır sayısı"));
        plan.ok = true;
        return plan;
    }

    if (sumish) {
        plan.intent = Intent::Sum;
        if (!numCol) {
            plan.explanation = QStringLiteral(
                "'%1' tablosunda toplanacak sayısal bir kolon bulamadim.").arg(table->name);
            return plan;
        }
        if (txtCol && containsAny(folded, {QStringLiteral("gor"), QStringLiteral("dagilim"),
                                           QStringLiteral("group"), QStringLiteral("bazinda"),
                                           QStringLiteral("per "), QStringLiteral("breakdown")})) {
            plan.sql = QStringLiteral("SELECT %1 AS kategori, SUM(%2) AS toplam FROM %3 GROUP BY %1 "
                                      "ORDER BY toplam DESC;")
                           .arg(q(txtCol->name), q(numCol->name), q(table->name));
            plan.explanation = QStringLiteral("'%1' kolonuna göre '%2' toplamı.")
                                   .arg(txtCol->name, numCol->name);
        } else {
            plan.sql = QStringLiteral("SELECT SUM(%1) AS toplam FROM %2;").arg(q(numCol->name), q(table->name));
            plan.explanation = QStringLiteral("'%1' kolonunun toplamı.").arg(numCol->name);
        }
        plan.ok = true;
        return plan;
    }

    if (topish) {
        plan.intent = Intent::TopN;
        if (!numCol || !txtCol) {
            plan.explanation = QStringLiteral(
                "'%1' tablosunda sıralama için uygun etiket/sayı kolonları bulunamadı.").arg(table->name);
            return plan;
        }
        const int n = findNumber(folded) > 0 ? findNumber(folded) : 10;
        plan.sql = QStringLiteral("SELECT %1, %2 FROM %3 ORDER BY %2 DESC LIMIT %4;")
                       .arg(q(txtCol->name), q(numCol->name), q(table->name)).arg(n);
        plan.explanation = QStringLiteral("'%1' kolonuna göre en yüksek ilk %2 listesi.")
                               .arg(numCol->name).arg(n);
        plan.ok = true;
        return plan;
    }

    // Lookup: quoted literal or a token matching a stored sample value.
    QString literal = quotedLiteral(question);
    const SchemaColumn *hitCol = nullptr;
    if (!literal.isEmpty()) {
        hitCol = schema.pickColumn(*table, [&](const SchemaColumn &c) -> int {
            for (const QString &s : c.samples) {
                if (foldTr(s).startsWith(foldTr(literal).left(3)))
                    return 10;
            }
            return acceptsText(c) ? 1 : 0;
        });
    } else {
        for (const SchemaColumn &c : table->columns) {
            if (acceptsText(c) && sampleHit(c, tokens)) {
                hitCol = &c;
                // Recover the sample value (true DB casing) that the token hit.
                for (const QString &tok : tokens) {
                    bool done = false;
                    for (const QString &s : c.samples) {
                        if (tok.size() >= 3 && foldLoose(s).startsWith(tok)) {
                            literal = s;
                            done = true;
                            break;
                        }
                    }
                    if (done)
                        break;
                }
                break;
            }
        }
    }
    if (hitCol && !literal.isEmpty()) {
        plan.intent = Intent::Lookup;
        const QString escaped = literal.replace(QStringLiteral("'"), QStringLiteral("''"));
        plan.sql = QStringLiteral("SELECT * FROM %1 WHERE %2 LIKE '%%3%';")
                       .arg(q(table->name), q(hitCol->name), escaped);
        plan.explanation = QStringLiteral("'%1' = '%2' kayıtları.").arg(hitCol->name, literal);
        plan.ok = true;
        return plan;
    }

    // Default: plain browse.
    plan.intent = Intent::Select;
    plan.sql = QStringLiteral("SELECT * FROM %1 LIMIT 50;").arg(q(table->name));
    plan.explanation = QStringLiteral("'%1' tablosunun ilk 50 satırı.").arg(table->name);
    plan.ok = true;
    return plan;
}

QString RuleBasedNlu::conversationalReply(const QString &question)
{
    const QString folded = foldLoose(question);
    const QStringList tokens = folded.split(QRegularExpression(QStringLiteral("[^\\p{L}0-9]+")),
                                            Qt::SkipEmptyParts);
    if (tokens.isEmpty() || tokens.size() > 10)
        return QString(); // never hijack a long/complex sentence

    // Every token must be "chat filler" for this to be pure small talk; a
    // single content word (a table/column name, a verb like 'goster') means
    // the user actually wants data.
    static const QSet<QString> chatTokens = {
        QStringLiteral("merhaba"), QStringLiteral("selam"), QStringLiteral("salem"),
        QStringLiteral("slm"), QStringLiteral("hey"), QStringLiteral("hello"),
        QStringLiteral("hi"), QStringLiteral("gunaydin"), QStringLiteral("iyigunler"),
        QStringLiteral("gunaydin"), QStringLiteral("iyi"), QStringLiteral("aksam"),
        QStringLiteral("iyiaksamlar"), QStringLiteral("nasilsin"), QStringLiteral("nasilsiniz"),
        QStringLiteral("naber"), QStringLiteral("nbr"), QStringLiteral("nasil"),
        QStringLiteral("gidiyor"), QStringLiteral("kimsin"), QStringLiteral("kim"),
        QStringLiteral("adin"), QStringLiteral("ne"), QStringLiteral("yapabilirsin"),
        QStringLiteral("yapabilir"), QStringLiteral("yardim"), QStringLiteral("help"),
        QStringLiteral("tesekkurler"), QStringLiteral("tesekkur"), QStringLiteral("sagol"),
        QStringLiteral("sagas"), QStringLiteral("saol"), QStringLiteral("thanks"),
        QStringLiteral("thank"), QStringLiteral("you"), QStringLiteral("eyvallah"),
        QStringLiteral("gorusuruz"), QStringLiteral("hosca"), QStringLiteral("hoscakal"),
        QStringLiteral("bay"), QStringLiteral("bye"), QStringLiteral("goodbye"),
        QStringLiteral("lutfen"), QStringLiteral("please"), QStringLiteral("tamam"),
        QStringLiteral("ok"), QStringLiteral("evet"), QStringLiteral("hayir"),
        QStringLiteral("misin"), QStringLiteral("musun"), QStringLiteral("misin"),
        QStringLiteral("varmi"), QStringLiteral("bir"), QStringLiteral("sey")};
    int content = 0;
    for (const QString &t : tokens) {
        if (!chatTokens.contains(t))
            ++content;
    }
    if (content > 0)
        return QString(); // has a real data intent -> leave it to the query path

    auto has = [&tokens](std::initializer_list<const char *> words) {
        for (const QString &t : tokens)
            for (const char *w : words)
                if (t == QLatin1String(w))
                    return true;
        return false;
    };

    const QString helpLine = QStringLiteral(
        "Ben QuickDb asistaninim: bagli veritabanindaki tablolar hakkinda dogal "
        "dille sorularini yanitlayip SQL uretir, sonuclari grafik/ozet halinde "
        "gosteririm. Ornek: 'satislar tablosunda aya gore toplam tutari goster' "
        "veya 'kac kullanici var'.");

    if (has({"tesekkurler", "tesekkur", "sagol", "sagas", "saol", "thanks", "thank", "eyvallah"}))
        return QStringLiteral("Rica ederim! Baska bir sorun olursa buradayim.");
    if (has({"gorusuruz", "hosca", "hoscakal", "bay", "bye", "goodbye"}))
        return QStringLiteral("Gorusuruz! Iyi calismalar dilerim.");
    if (has({"kimsin", "kim", "adin"}))
        return QStringLiteral("Ben QuickDb - veritabani asistaninim. ") + helpLine;
    if (has({"yardim", "help", "yapabilirsin", "yapabilir"}))
        return helpLine;
    if (has({"nasilsin", "nasilsiniz", "naber", "nbr", "merhaba", "selam", "salem", "slm",
             "hello", "hi", "hey", "gunaydin", "iyigunler", "iyiaksamlar"}))
        return QStringLiteral("Merhaba! ") + helpLine;

    return QString();
}

} // namespace ai
