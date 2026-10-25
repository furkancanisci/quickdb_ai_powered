#include "ai/AiOrchestrator.h"

#include <QMetaType>
#include <QRegularExpression>
#include <QSettings>

namespace ai {

namespace {
const char *kSourceLlm = "LLM (Ollama)";
const char *kSourceRule = "Kural Motoru";

// Small/fast default analyst model; overridable via QSettings "analystModel".
const char *kDefaultAnalystModel = "qwen2.5:1.5b";

// Prose system prompt for the spec-compliance self-review (must NOT emit SQL).
const char *kVerifySystemPrompt =
    "You are a careful SQL result reviewer. Compare a SQLite query's RESULT "
    "against the user's original request. Reply in short plain text only, never "
    "write SQL. If the result fully satisfies the request, reply exactly 'PASS'. "
    "Otherwise reply 'GAPS:' followed by a short list of what is missing or wrong.";

// Senior business-analyst system prompt (TR): interpreted insight, not raw data.
const char *kAnalystSystemPrompt =
    "Sen kiydemli bir is analistisin. Yoneticiye ham veriyi degil, yorumlanmis "
    "icgoru ver: kiyaslama, oran, en yuksek/en dusuk, trend. 2-4 kisa cumle, "
    "somut sayilarla (%25 gibi). KESINLIKLE yalnizca sana verilen kolon adlarini "
    "ve satir degerlerini kullan; veride OLMAYAN yil, tarih, kisi, birim veya "
    "senaryo UYDURMA (halusinasyon yasak). Yorumlayacak anlamli sayisal veri yoksa "
    "mevcut istatistigini tekrar et, kelime tekrarina dusme. SQL yazma, sadece "
    "Turkce yonetici ozeti ver.";

QString clipSql(const QString &sql, int n = 1600)
{
    return sql.length() > n ? sql.left(n) + QStringLiteral("\n-- ... (kesildi) ...") : sql;
}

// Single-line, markdown-free analyst reply (models love fences/bullets).
QString tidyProse(const QString &raw)
{
    QString t = raw.trimmed();
    t.replace(QStringLiteral("```"), QString());
    while (t.startsWith(QLatin1Char('#')) || t.startsWith(QLatin1Char('>')))
        t = t.mid(1).trimmed();
    // The 1.5b model answers with "1. **bold**" lists; flatten to prose.
    t.replace(QStringLiteral("**"), QString());
    t.replace(QRegularExpression(QStringLiteral("(?:^|\\s)\\d+[.)\\]]\\s*")), QStringLiteral(" "));
    t.replace(QStringLiteral("\u2022"), QStringLiteral(" ")); // bullet
    t.replace(QStringLiteral("\n"), QStringLiteral(" "));
    while (t.contains(QStringLiteral("  ")))
        t.replace(QStringLiteral("  "), QStringLiteral(" "));
    return t.trimmed();
}

// JSON string escaping for the compact analyst table (keeps quotes/backslashes).
QString jsonEsc(const QString &in)
{
    QString out;
    out.reserve(in.size() + 4);
    for (const QChar &c : in) {
        if (c == QLatin1Char('\\'))
            out += QLatin1String("\\\\");
        else if (c == QLatin1Char('"'))
            out += QLatin1String("\\\"");
        else if (c == QLatin1Char('\n') || c == QLatin1Char('\r'))
            out += QLatin1Char(' ');
        else
            out += c;
    }
    return out;
}
} // namespace

AiOrchestrator::AiOrchestrator(core::DatabaseManager *db, QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_ollama(new OllamaClient(this))
{
    // Register the Answer metatype once so it can travel through queued
    // signal/slot connections (used by the headless test harness too).
    static const int typeId = qRegisterMetaType<Answer>("ai::AiOrchestrator::Answer");
    Q_UNUSED(typeId);

    connect(m_ollama, &OllamaClient::generationFinished, this,
            [this](bool ok, const QString &raw, const QString &error) {
                onGenerated(ok, raw, error);
            });

    // Second model: the business analyst. Own instance, own model name, own
    // availability probe; only generate() is ever called on it.
    QSettings settings;
    m_analyst = new OllamaClient(this);
    m_analyst->setModel(settings.value(QStringLiteral("analystModel"),
                                        QLatin1String(kDefaultAnalystModel))
                            .toString());
    m_useAnalyst = settings.value(QStringLiteral("analystEnabled"), true).toBool();
    connect(m_analyst, &OllamaClient::generationFinished, this,
            [this](bool ok, const QString &raw, const QString &error) {
                onAnalystFinished(ok, raw, error);
            });
    m_analyst->probe(); // fire-and-forget, like the SQL model's probe
}

QString AiOrchestrator::sqlPrompt(const QString &question)
{
    const SchemaContext ctx = SchemaContext::fromDatabase(*m_db);
    const QString dialect = dialectHint();
    return QStringLiteral(
        "%1 schema:\n%2\n\nQuestion (TR/EN): %3\n\n"
        "Write one read-only SQL query answering the question.")
        .arg(dialect, ctx.toPromptText(), question);
}

QString AiOrchestrator::dialectHint() const
{
    // The schema renderer is driver-agnostic, but the literal SQL dialect and
    // the introspection idiom differ, so name the target engine explicitly.
    return m_db && !m_db->currentConfig().isSqlite()
               ? QStringLiteral("PostgreSQL (use information_schema, double-quoted "
                                "identifiers, standard SQL)")
               : QStringLiteral("SQLite");
}

QString AiOrchestrator::repairPrompt(const QString &question, const QString &failedSql,
                                     const QString &error)
{
    const SchemaContext ctx = SchemaContext::fromDatabase(*m_db);
    return QStringLiteral(
        "The SQL below was generated for the question but FAILED to execute against "
        "the %1 database. Fix ONLY what is needed to make it run (correct column/"
        "table names, parentheses, joins) while keeping every requested output column. "
        "Do not simplify it to a trivial query. The failing SQL may be truncated because "
        "it is long, so output the COMPLETE corrected statement INCLUDING its WITH/CTE "
        "header - never a fragment that starts mid-query.\n\n"
        "%1 schema:\n%2\n\nOriginal question (TR/EN): %3\n\n"
        "Failing SQL:\n%4\n\nDatabase error: %5\n\n"
        "Reply with only the corrected read-only SQL inside a ```sql fence.")
        .arg(dialectHint(), ctx.toPromptText(), question, failedSql, error);
}

QString AiOrchestrator::verifyPrompt(const Answer &a)
{
    QStringList colList;
    for (int c = 0; c < a.data.columns.size(); ++c)
        colList << a.data.columns.at(c);
    QString sample;
    if (!a.data.rows.isEmpty()) {
        for (int c = 0; c < a.data.columns.size() && c < 8; ++c)
            sample += QStringLiteral("%1=%2; ").arg(a.data.columns.at(c), a.data.rows.at(0).value(c));
    }
    return QStringLiteral(
        "A query was run to answer this request. Verify the RESULT matches what was "
        "requested.\n\nRequest (TR/EN): %1\n\nSQL (possibly truncated): %2\n\n"
        "Result column count: %3\nResult columns: %4\nRow count: %5\nFirst row (sample): %6\n\n"
        "Does the result deliver everything the request asked for?")
        .arg(a.question, clipSql(a.generatedSql))
        .arg(a.data.columns.size())
        .arg(colList.join(QStringLiteral(", ")))
        .arg(a.data.rows.size())
        .arg(sample.isEmpty() ? QStringLiteral("(no rows)") : sample);
}

QString AiOrchestrator::analystPrompt(const Answer &a) const
{
    // Compact JSON-ish table: column names + at most 15 rows + the numeric
    // statistics the deterministic inspector already computed.
    QStringList colList;
    for (const QString &c : a.data.columns)
        colList << QStringLiteral("\"%1\"").arg(jsonEsc(c));
    QString rows;
    const int show = qMin(15, a.data.rows.size());
    for (int r = 0; r < show; ++r) {
        QStringList cells;
        for (int c = 0; c < a.data.columns.size(); ++c)
            cells << QStringLiteral("\"%1\":\"%2\"")
                         .arg(jsonEsc(a.data.columns.at(c)),
                              jsonEsc(a.data.rows.at(r).value(c)));
        rows += QStringLiteral("  {%1}\n").arg(cells.join(QStringLiteral(", ")));
    }
    if (a.data.rows.size() > show)
        rows += QStringLiteral("  ... (%1 satir daha)\n").arg(a.data.rows.size() - show);

    return QStringLiteral(
        "Soru (TR/EN): %1\n\nSorgu sonucunun kolonlari: [%2]\nVeri satirlari (%3 satir):\n%4"
        "\nOtomatik istatistik ozeti: %5\n\n"
        "Bu veriyi yonetici icin 2-4 kisa cumleyle yorumla: karsilastirma, oran, "
        "en yuksek/en dusuk kalem ve trend. Somut sayilari veriden kullan. "
        "ONEMLI: Yalnizca yukaridaki kolon adlarini ve degerlerini referans al; "
        "veride olmayan yil/tarih/kisi/senaryo UYDURMA.")
        .arg(a.question, colList.join(QStringLiteral(", ")))
        .arg(a.data.rows.size())
        .arg(rows, a.insight.summaryTr.isEmpty() ? QStringLiteral("-") : a.insight.summaryTr);
}

void AiOrchestrator::dispatchAnswer(const Answer &a)
{
    // The analyst stage belongs to the interactive LLM pipeline only: the
    // deterministic offline path must stay synchronous for tests/tooling.
    // It also only makes sense when there is a real numeric measure over more
    // than one row - running the small model on a lone text value (e.g.
    // SELECT 'Merhaba!') or an id/count-only table just produces hallucination.
    const bool interpretable = a.insight.yColumn >= 0 && a.data.rows.size() >= 2;
    if (m_useLlm && m_useAnalyst && a.executed && interpretable && m_analyst
        && m_analyst->isAvailable()) {
        runAnalyst(a);
        return;
    }
    Answer out = a;
    applyAnalystFallback(out, QString());
    emit answerReady(out);
}

void AiOrchestrator::runAnalyst(const Answer &a)
{
    m_stash = a;
    m_phase = AwaitingAnalyst;
    if (m_statusCb)
        m_statusCb(QStringLiteral("Analist modeli yorumluyor..."));
    m_analyst->generate(QLatin1String(kAnalystSystemPrompt), analystPrompt(a));
}

void AiOrchestrator::onAnalystFinished(bool ok, const QString &raw, const QString &error)
{
    m_phase = Idle;
    Answer a = m_stash;
    const QString text = tidyProse(raw);
    if (ok && !text.isEmpty()) {
        a.analystText = text;
        a.analystSource = QStringLiteral("Analist (%1)").arg(m_analyst->model());
    } else {
        applyAnalystFallback(a, error.isEmpty() ? QStringLiteral("model bos dondu") : error);
    }
    emit answerReady(a);
}

void AiOrchestrator::applyAnalystFallback(Answer &a, const QString &reason) const
{
    // Honest degradation: the deterministic insight text stands in, and the
    // source field says which model could not interpret the data.
    a.analystText = a.insight.summaryTr;
    a.analystSource = reason.isEmpty()
        ? QStringLiteral("Analist modeli kapali/dostlugu yok; otomatik istatistik özeti kullanildi.")
        : QStringLiteral("Analist modeli yorumlayamadi (%1); otomatik istatistik özeti kullanildi.")
              .arg(reason);
}

void AiOrchestrator::ask(const QString &question)
{
    m_pendingQuestion = question;
    m_attempt = 1;
    m_repairsDone = 0;
    m_phase = Idle;

    // Conversational intent first: greetings / thanks / how-are-you / help are
    // answered in prose and never sent down the SQL + analyst pipeline.
    const QString chat = RuleBasedNlu::conversationalReply(question);
    if (!chat.isEmpty()) {
        Answer a;
        a.question = question;
        a.conversational = true;
        a.source = QStringLiteral("Sohbet");
        a.message = chat;
        emit answerReady(a);
        return;
    }

    if (!m_db || !m_db->isConnected()) {
        Answer a;
        a.question = question;
        a.message = QStringLiteral("Bagli veritabani yok. Once bir DB olusturun/acinn.");
        emit answerReady(a);
        return;
    }

    // Preferred path: local LLM (only when enabled and Ollama is up).
    if (m_useLlm && m_ollama->isAvailable()) {
        m_phase = AwaitingSql;
        m_ollama->generateSql(sqlPrompt(question));
        return;
    }

    // Offline deterministic path.
    fallbackToRules(question);
}

void AiOrchestrator::onGenerated(bool ok, const QString &raw, const QString &error)
{
    // ---- Verification self-review finished --------------------------------
    if (m_phase == AwaitingVerify) {
        m_phase = Idle;
        if (ok) {
            const QString t = raw.trimmed();
            if (t.startsWith(QStringLiteral("PASS"), Qt::CaseInsensitive))
                m_stash.verificationNote =
                    QStringLiteral("Model dogruladi: istenen karsilandi (PASS).");
            else
                m_stash.verificationNote = QStringLiteral("Uyusma kontrolu: %1").arg(t);
        } else {
            m_stash.verificationNote =
                QStringLiteral("Uyusma kontrolu atlandi (model yanit vermedi: %1).").arg(error);
        }
        dispatchAnswer(m_stash); // analyst stage (or direct emit) follows
        return;
    }

    // ---- A SQL generation finished (initial or a repair retry) ------------
    Q_UNUSED(error);
    const QString sql = ok ? RuleBasedNlu::extractSqlFromText(raw) : QString();

    if (!sql.isEmpty() && RuleBasedNlu::isReadOnlySql(sql)) {
        executeSql(sql);
        return;
    }

    // Model produced no usable (or unsafe/empty) SQL this round.
    if (m_selfRepair && m_repairsDone < m_maxRepair) {
        ++m_repairsDone;
        ++m_attempt;
        emit retryProgress(m_attempt, m_maxRepair + 1,
                           QStringLiteral("Model gecersiz/guvenli olmayan SQL uretti; yeniden deneniyor..."));
        m_phase = AwaitingSql;
        m_ollama->generateSql(sqlPrompt(m_pendingQuestion));
    } else {
        // Nothing to show; offline deterministic fallback.
        fallbackToRules(m_pendingQuestion);
    }
}

void AiOrchestrator::executeSql(const QString &sql)
{
    m_lastSql = sql;
    const core::QueryResult result = m_db->runSingle(sql);

    if (result.success && result.hasResultSet) {
        QString msg = QStringLiteral("Yerel model SQL uretti.");
        if (m_repairsDone > 0)
            msg += QStringLiteral(" (%1 hatadan sonra duzeltildi)").arg(m_repairsDone);
        Answer a = finish(m_pendingQuestion, sql, QString::fromUtf8(kSourceLlm), msg, result);
        a.attempts = m_attempt;
        a.repairAttempts = m_repairsDone;
        if (m_verifySpec && m_useLlm && m_ollama->isAvailable()) {
            requestVerification(a);
        } else {
            dispatchAnswer(a); // analyst interpretation stage
        }
        return;
    }

    // Execution failed: self-repair if budget remains, else honest give-up.
    if (m_selfRepair && m_repairsDone < m_maxRepair) {
        requestRepair(sql, result.errorMessage);
    } else {
        Answer a = finish(m_pendingQuestion, sql, QString::fromUtf8(kSourceLlm),
                          QStringLiteral("Model %1 denemede de calistirilabilir SQL uretemedi; "
                                         "son SQL ve gercek hata asagida.")
                              .arg(m_attempt),
                          result);
        a.attempts = m_attempt;
        a.repairAttempts = m_repairsDone;
        emit answerReady(a); // a.executed stays false -> transparency bubble
    }
}

void AiOrchestrator::requestRepair(const QString &failedSql, const QString &error)
{
    ++m_repairsDone;
    ++m_attempt;
    emit retryProgress(m_attempt, m_maxRepair + 1, error);
    m_phase = AwaitingSql;
    m_ollama->generateSql(repairPrompt(m_pendingQuestion, failedSql, error));
}

void AiOrchestrator::requestVerification(const Answer &successful)
{
    m_stash = successful;
    m_phase = AwaitingVerify;
    m_ollama->generate(QLatin1String(kVerifySystemPrompt), verifyPrompt(successful));
}

void AiOrchestrator::fallbackToRules(const QString &question)
{
    const SchemaContext ctx = SchemaContext::fromDatabase(*m_db);
    const RuleBasedNlu::Plan plan = RuleBasedNlu::run(question, ctx);
    if (!plan.ok || plan.sql.isEmpty()) {
        Answer a;
        a.question = question;
        a.message = plan.explanation.isEmpty()
            ? QStringLiteral("Sorgu uretilemedi; SQL'i elle yazmayi deneyin.")
            : plan.explanation;
        emit answerReady(a);
        return;
    }
    runSql(question, plan.sql, QString::fromUtf8(kSourceRule), plan.explanation);
}

void AiOrchestrator::runSql(const QString &question, const QString &sql,
                            const QString &source, const QString &message)
{
    m_lastSql = sql;
    const core::QueryResult result = m_db->runSingle(sql);
    dispatchAnswer(finish(question, sql, source, message, result));
}

AiOrchestrator::Answer AiOrchestrator::finish(const QString &question, const QString &sql,
                                              const QString &source, const QString &message,
                                              const core::QueryResult &result)
{
    Answer a;
    a.question = question;
    a.generatedSql = sql;
    a.source = source;
    a.message = message;
    a.result = result; // struct copy: QSqlQuery handle is reference-counted

    if (result.success && result.hasResultSet) {
        a.executed = true;
        a.data = TableData::fromQueryResult(result);
        a.insight = DataInspector::analyze(a.data);
    }
    return a;
}

bool AiOrchestrator::reRunLast()
{
    if (m_lastSql.isEmpty() || !m_db || !m_db->isConnected())
        return false;
    runSql(QString(), m_lastSql, QString(), QStringLiteral("Tekrar calistirildi."));
    return true;
}

} // namespace ai
