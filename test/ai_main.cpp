// Offline unit test for the ai/ logic layer: rule engine, SQL guard/extract,
// schema matching and data inspector. No network, no widgets.
#include <QCoreApplication>
#include <QString>
#include <QTemporaryDir>
#include <QVector>

#include "ai/AiOrchestrator.h"
#include "ai/DataInspector.h"
#include "ai/RuleBasedNlu.h"
#include "ai/SchemaContext.h"
#include "core/DatabaseManager.h"

static int failures = 0;
static void check(bool cond, const QString &label)
{
    qInfo().noquote() << (cond ? QStringLiteral("[PASS] ") : QStringLiteral("[FAIL] ")) + label;
    if (!cond)
        ++failures;
}

static ai::SchemaContext sampleSchema()
{
    using namespace ai;
    SchemaContext ctx;
    SchemaTable satis;
    satis.name = QStringLiteral("satislar");
    satis.columns = {
        {QStringLiteral("id"), QStringLiteral("INTEGER"), true, {}}};
    satis.columns.append({QStringLiteral("ay"), QStringLiteral("TEXT"), false,
                          {QStringLiteral("2025-01"), QStringLiteral("2025-02")}});
    satis.columns.append({QStringLiteral("tutar"), QStringLiteral("REAL"), false,
                          {QStringLiteral("1200"), QStringLiteral("1500")}});
    ctx.tables.append(satis);

    SchemaTable users;
    users.name = QStringLiteral("kullanicilar");
    users.columns = {
        {QStringLiteral("id"), QStringLiteral("INTEGER"), true, {}},
        {QStringLiteral("ad"), QStringLiteral("TEXT"), false,
         {QStringLiteral("furkan"), QStringLiteral("ali")}},
        {QStringLiteral("yas"), QStringLiteral("INTEGER"), false,
         {QStringLiteral("25"), QStringLiteral("30")}}};
    ctx.tables.append(users);
    return ctx;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    using namespace ai;

    // ---- SchemaContext matching ------------------------------------------
    const SchemaContext schema = sampleSchema();
    check(schema.bestTableForQuestion(QStringLiteral("satislar icin trend")) != nullptr
            && schema.bestTableForQuestion(QStringLiteral("satislar icin trend"))->name
                   == QStringLiteral("satislar"),
          QStringLiteral("bestTableForQuestion finds 'satislar'"));
    check(schema.bestTableForQuestion(QStringLiteral("kullanicilarin yasi")) != nullptr,
          QStringLiteral("suffix-fold matches 'kullanicilarin' -> kullanicilar"));
    check(SchemaContext::looksNumeric(QStringLiteral("INTEGER")),
          QStringLiteral("looksNumeric(INTEGER)"));

    // ---- RuleBasedNlu intent -> SQL --------------------------------------
    {
        auto plan = RuleBasedNlu::run(QStringLiteral("kac tane kayit var"), schema);
        check(plan.ok && plan.tableName == QStringLiteral("satislar")
                  && plan.sql.contains(QStringLiteral("COUNT(*)")),
              QStringLiteral("count intent -> COUNT(*) on default table: %1").arg(plan.sql));
    }
    {
        auto plan = RuleBasedNlu::run(QStringLiteral("Son 6 ayin satis trendini goster"), schema);
        check(plan.ok && plan.intent == RuleBasedNlu::Intent::Trend
                  && plan.sql.contains(QStringLiteral("GROUP BY"))
                  && plan.sql.contains(QStringLiteral("2025")) == false
                  && plan.sql.contains(QStringLiteral("tutar")),
              QStringLiteral("trend intent -> GROUP BY sum(tutar): %1").arg(plan.sql));
    }
    {
        auto plan = RuleBasedNlu::run(QStringLiteral("toplam tutar"), schema);
        check(plan.ok && plan.sql.contains(QStringLiteral("SUM(")) && plan.sql.contains(QStringLiteral("tutar")),
              QStringLiteral("sum intent -> SUM(tutar): %1").arg(plan.sql));
    }
    {
        auto plan = RuleBasedNlu::run(QStringLiteral("en yuksek 5 tutar"), schema);
        check(plan.ok && plan.sql.contains(QStringLiteral("ORDER BY")) && plan.sql.contains(QStringLiteral("LIMIT 5")),
              QStringLiteral("topN intent -> ORDER BY DESC LIMIT 5: %1").arg(plan.sql));
    }
    {
        auto plan = RuleBasedNlu::run(QStringLiteral("\"furkan\" kullanicisini bul"), schema);
        check(plan.ok && plan.intent == RuleBasedNlu::Intent::Lookup
                  && plan.sql.contains(QStringLiteral("LIKE")) && plan.sql.contains(QStringLiteral("furkan")),
              QStringLiteral("lookup intent -> LIKE 'furkan': %1").arg(plan.sql));
    }
    {
        // English variants still work (folding + keyword lists).
        auto plan = RuleBasedNlu::run(QStringLiteral("how many rows in satislar"), schema);
        check(plan.ok && plan.sql.contains(QStringLiteral("COUNT(*)")),
              QStringLiteral("EN count recognized: %1").arg(plan.sql));
    }
    {
        auto plan = RuleBasedNlu::run(QStringLiteral("bilinmeyen sey"), schema);
        // Unknown wording on a multi-table schema without a table hint:
        // falls back to a plain SELECT on the single best guess OR asks to clarify.
        check(plan.ok || !plan.explanation.isEmpty(),
              QStringLiteral("unknown question yields plan or clarification"));
    }

    // ---- SQL safety guard + extraction -----------------------------------
    check(RuleBasedNlu::isReadOnlySql(QStringLiteral("SELECT * FROM t;")),
          QStringLiteral("SELECT allowed"));
    check(RuleBasedNlu::isReadOnlySql(QStringLiteral("WITH x AS (SELECT 1) SELECT * FROM x")),
          QStringLiteral("WITH allowed"));
    check(!RuleBasedNlu::isReadOnlySql(QStringLiteral("DROP TABLE t;")),
          QStringLiteral("DROP rejected"));
    check(!RuleBasedNlu::isReadOnlySql(QStringLiteral("SELECT * FROM t; DELETE FROM t")),
          QStringLiteral("stacked write rejected"));
    check(!RuleBasedNlu::isReadOnlySql(QStringLiteral("INSERT INTO t VALUES (1)")),
          QStringLiteral("INSERT rejected"));
    {
        const QString fenced = QStringLiteral(
            "Sure, here it is:\n```sql\nSELECT ay, SUM(tutar) FROM satislar GROUP BY ay;\n```\nDone.");
        const QString sql = RuleBasedNlu::extractSqlFromText(fenced);
        check(sql.startsWith(QStringLiteral("SELECT")) && sql.contains(QStringLiteral("GROUP BY"))
                  && !sql.contains(QStringLiteral("```")),
              QStringLiteral("extractSqlFromText pulls SQL from fence: %1").arg(sql));
        check(RuleBasedNlu::isReadOnlySql(sql), QStringLiteral("extracted SQL passes read-only guard"));
    }
    {
        // Regression: a fenced WITH/CTE query must be kept WHOLE - the old
        // SELECT-span pull dropped the CTE header and produced a fragment
        // starting mid-query (SQLite: near ")": syntax error).
        const QString fenced = QStringLiteral(
            "```sql\nWITH t AS (SELECT id FROM users)\nSELECT COUNT(*) FROM t;\n```");
        const QString sql = RuleBasedNlu::extractSqlFromText(fenced);
        check(sql.startsWith(QStringLiteral("WITH"), Qt::CaseInsensitive)
                  && sql.contains(QStringLiteral("WITH t AS"))
                  && sql.contains(QStringLiteral("SELECT COUNT(*) FROM t")),
              QStringLiteral("WITH/CTE extraction keeps the full query, not a SELECT fragment: %1")
                  .arg(sql));
        check(RuleBasedNlu::isReadOnlySql(sql), QStringLiteral("extracted WITH query passes guard"));
    }

    // ---- DataInspector chart decision + summary --------------------------
    {
        TableData d;
        d.columns = {QStringLiteral("ay"), QStringLiteral("tutar")};
        d.rows = {
            {QStringLiteral("2025-01"), QStringLiteral("100")},
            {QStringLiteral("2025-02"), QStringLiteral("150")},
            {QStringLiteral("2025-03"), QStringLiteral("120")},
            {QStringLiteral("2025-04"), QStringLiteral("200")},
            {QStringLiteral("2025-05"), QStringLiteral("180")},
            {QStringLiteral("2025-06"), QStringLiteral("260")}};
        Insight ins = DataInspector::analyze(d);
        check(ins.chartType == ChartType::Line && ins.xColumn == 0 && ins.yColumn == 1,
              QStringLiteral("date trend -> Line, x=0 y=1 (got %1/%2/%3)")
                  .arg(chartTypeName(ins.chartType)).arg(ins.xColumn).arg(ins.yColumn));
        check(ins.summaryTr.contains(QStringLiteral("en yuksek")) && ins.summaryTr.contains(QStringLiteral("260")),
              QStringLiteral("TR summary mentions peak: %1").arg(ins.summaryTr));
        check(!ins.summaryEn.isEmpty(), QStringLiteral("EN summary produced"));
        // Analyst report: multi-section HTML with KPIs, distribution, recommendations.
        check(ins.reportHtmlTr.contains(QStringLiteral("Temel Metrikler"))
                  && ins.reportHtmlTr.contains(QStringLiteral("Dagilim"))
                  && ins.reportHtmlTr.contains(QStringLiteral("Oneriler")),
              QStringLiteral("TR analyst report has KPI/Distribution/Recommendations sections"));
        check(ins.reportHtmlTr.contains(QStringLiteral("260"))
                  && ins.reportHtmlTr.contains(QStringLiteral("%")),
              QStringLiteral("TR report includes peak value + share percentage"));
        check(ins.reportHtmlEn.contains(QStringLiteral("Executive Summary"))
                  && ins.reportHtmlEn.contains(QStringLiteral("Distribution")),
              QStringLiteral("EN analyst report sections present"));
    }
    {
        TableData d;
        d.columns = {QStringLiteral("kategori"), QStringLiteral("adet")};
        d.rows = {{QStringLiteral("A"), QStringLiteral("10")},
                  {QStringLiteral("B"), QStringLiteral("20")},
                  {QStringLiteral("C"), QStringLiteral("30")}};
        Insight ins = DataInspector::analyze(d);
        check(ins.chartType == ChartType::Pie,
              QStringLiteral("small categorical -> Pie (got %1)").arg(chartTypeName(ins.chartType)));
    }
    {
        TableData d;
        d.columns = {QStringLiteral("kategori"), QStringLiteral("adet")};
        for (int i = 0; i < 12; ++i)
            d.rows.append({QStringLiteral("k%1").arg(i), QString::number(i * 3)});
        Insight ins = DataInspector::analyze(d);
        check(ins.chartType == ChartType::Bar,
              QStringLiteral("many categories -> Bar (got %1)").arg(chartTypeName(ins.chartType)));
    }

    // ---- DataInspector scalar / aggregate detection -----------------------
    {
        TableData d; // SELECT SUM(...) style: one row, one numeric column
        d.columns = {QStringLiteral("toplam_tutar")};
        d.rows = {{QStringLiteral("42000")}};
        Insight ins = DataInspector::analyze(d);
        check(ins.isScalar && ins.chartType == ChartType::None && !ins.plottable(),
              QStringLiteral("single-row aggregate -> scalar, not a chart"));
        check(ins.kpiLabel == QStringLiteral("toplam_tutar")
                  && ins.kpiValue == QStringLiteral("42000"),
              QStringLiteral("scalar KPI headline label/value captured"));
        check(!ins.summaryTr.contains(QStringLiteral("()"))
                  && !ins.summaryEn.contains(QStringLiteral("()")),
              QStringLiteral("scalar summary has no empty () parens"));
        check(!ins.reportHtmlTr.contains(QStringLiteral("Dagilim"))
                  && !ins.reportHtmlTr.contains(QStringLiteral("Oneriler"))
                  && !ins.reportHtmlTr.contains(QStringLiteral("#1)")),
              QStringLiteral("single-row report omits fake Distribution/Recommendations/#1"));
        check(ins.reportHtmlTr.contains(QStringLiteral("Tekil Metrik"))
                  && ins.reportHtmlEn.contains(QStringLiteral("Single Metric")),
              QStringLiteral("single-row report shows the metric section (TR+EN)"));
    }
    {
        // Multiple rows but no categorical axis (single numeric column): the
        // peak label is empty, so the summary must not print a bare "()".
        TableData d;
        d.columns = {QStringLiteral("deger")};
        for (int i = 1; i <= 6; ++i)
            d.rows.append({QString::number(i * 10)});
        Insight ins = DataInspector::analyze(d);
        check(!ins.isScalar && !ins.summaryTr.contains(QStringLiteral("()"))
                  && !ins.summaryEn.contains(QStringLiteral("()")),
              QStringLiteral("multi-row no-axis summary avoids empty ()"));
    }

    // ---- End-to-end through DatabaseManager (no network) -----------------
    {
        QTemporaryDir dir;
        const QString path = dir.path() + QStringLiteral("/ai_test.db");
        core::DatabaseManager db;
        QString err;
        const bool seeded = db.createDatabase(path, &err)
                  && db.runScript(QStringLiteral(
                         "CREATE TABLE satislar (id INTEGER PRIMARY KEY, ay TEXT, tutar REAL);\n"
                         "INSERT INTO satislar (ay,tutar) VALUES "
                         "('2025-01',100),('2025-02',150),('2025-03',120),('2025-04',200),"
                         "('2025-05',180),('2025-06',260);"))
                        .success;
        check(seeded, QStringLiteral("fixture DB created (%1)").arg(err));

        // Orchestrator with LLM disabled must use the rule engine offline.
        ai::AiOrchestrator orch(&db);
        orch.setUseLlm(false);
        ai::AiOrchestrator::Answer got;
        bool received = false;
        QObject::connect(&orch, &ai::AiOrchestrator::answerReady,
                         [&](const ai::AiOrchestrator::Answer &a) { got = a; received = true; });
        orch.ask(QStringLiteral("satislar icin aya gore toplam tutar trendini goster"));
        QCoreApplication::processEvents();
        check(received && got.executed && got.source.contains(QStringLiteral("Kural"))
                  && !got.data.isEmpty() && got.insight.plottable(),
              QStringLiteral("orchestrator offline -> rule engine executed + plottable insight (%1)")
                  .arg(got.generatedSql));
        // Analyst stage is an LLM-pipeline-only step: the deterministic path
        // must still fill the exec-summary fields (honest fallback, no model).
        check(!got.analystText.isEmpty() && !got.analystSource.isEmpty(),
              QStringLiteral("analyst fields fall back to the deterministic summary offline (%1)")
                  .arg(got.analystSource));
        check(got.analystText == got.insight.summaryTr,
              QStringLiteral("offline analyst text equals the insight summary"));
        db.closeDatabase();
    }

    // ---- Conversational intent: greetings are small talk, never SQL -------
    {
        check(!RuleBasedNlu::conversationalReply(QStringLiteral("merhaba")).isEmpty(),
              QStringLiteral("'merhaba' detected as small talk (answered in prose)"));
        check(!RuleBasedNlu::conversationalReply(QStringLiteral("selam nasilsin")).isEmpty(),
              QStringLiteral("'selam nasilsin' detected as small talk"));
        check(RuleBasedNlu::conversationalReply(
                  QStringLiteral("satislar tablosunda aya gore toplam tutar")).isEmpty(),
              QStringLiteral("a real data question is NOT treated as small talk"));
        check(RuleBasedNlu::conversationalReply(QStringLiteral("kac kullanici var")).isEmpty(),
              QStringLiteral("a count question is NOT treated as small talk"));
    }

    // ---- Identifier columns are never a Y-axis metric (count-mode) --------
    {
        TableData d;
        d.columns = {QStringLiteral("id"), QStringLiteral("name")};
        d.rows = {{QStringLiteral("1"), QStringLiteral("furkan")},
                  {QStringLiteral("2"), QStringLiteral("can")}};
        check(DataInspector::columnLooksIdentifier(d, 0),
              QStringLiteral("consecutive 'id' column recognized as an identifier"));
        Insight ins = DataInspector::analyze(d);
        check(ins.yColumn < 0 && !ins.isScalar,
              QStringLiteral("id/name table yields count-mode, id is never a metric"));
        // Equal 50/50 split must NOT be flagged as high concentration.
        check(!ins.reportHtmlTr.contains(QStringLiteral("Yogunlasma yuksek")),
              QStringLiteral("even 2-way distribution is not labelled high concentration"));
    }

    qInfo().noquote() << (failures == 0 ? QStringLiteral("AI TEST: ALL PASSED")
                                        : QStringLiteral("AI TEST: %1 FAILURE(S)").arg(failures));
    return failures;
}
