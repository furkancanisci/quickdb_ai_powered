// Live end-to-end integration driver: connects a REAL SQLite schema, runs the
// production ai::AiOrchestrator pipeline against the REAL local Ollama model
// (qwen2.5-coder:3b), executes the generated SQL and materializes the
// DataInspector insight. Also exercises the SELF-REPAIR loop (feed a DB error
// back to the model and retry) and the spec-compliance VERIFICATION pass.
// This is the exact chain the GUI drives:
//   prompt -> model -> generate SQL -> run SQL -> (repair on error) -> result
//          -> analytics -> spec check.
// Requires Ollama running on localhost:11434 with the model available.
//
// Usage: e2e_ai_test /abs/path/to/company_analytics.db
#include <QCoreApplication>
#include <QEventLoop>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include "ai/AiOrchestrator.h"
#include "ai/DataInspector.h"
#include "core/DatabaseManager.h"

static int failures = 0;
static void check(bool cond, const QString &label)
{
    qInfo().noquote() << (cond ? QStringLiteral("[PASS] ") : QStringLiteral("[FAIL] ")) + label;
    if (!cond)
        ++failures;
}

static QString preview(const QString &s, int n = 260)
{
    QString t = s;
    t.replace(QLatin1Char('\n'), QLatin1Char(' ')).replace(QLatin1Char('\r'), QLatin1Char(' '));
    return t.length() > n ? t.left(n) + QStringLiteral("...") : t;
}

// Retries observed for the question currently in flight.
static int g_retries = 0;

static ai::AiOrchestrator::Answer askBlocking(ai::AiOrchestrator &orch,
                                              const QString &question,
                                              bool &timedOut,
                                              int timeoutMs = 240000)
{
    ai::AiOrchestrator::Answer got;
    bool done = false;
    g_retries = 0;
    QEventLoop loop;
    const auto conn = QObject::connect(&orch, &ai::AiOrchestrator::answerReady,
                     [&](const ai::AiOrchestrator::Answer &a) {
                         got = a;
                         done = true;
                         loop.quit();
                     });
    QTimer watchdog;
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);
    watchdog.start(timeoutMs);
    timedOut = false;
    orch.ask(question);
    loop.exec();
    QObject::disconnect(conn); // avoid stale captures firing on later questions
    if (!done)
        timedOut = true;
    return got;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() < 2) {
        qWarning() << "usage: e2e_ai_test <path/to/database.db>";
        return 2;
    }
    const QString dbPath = args.at(1);

    core::DatabaseManager db;
    QString err;
    if (!db.openDatabase(dbPath, &err)) {
        qWarning().noquote() << "cannot open DB:" << err;
        return 2;
    }
    qInfo().noquote() << "Connected:" << dbPath
                      << "| tables:" << db.tableNames().join(QStringLiteral(", "));

    ai::AiOrchestrator orch(&db);
    orch.setUseLlm(true);
    orch.setSelfRepair(true);
    orch.setMaxRepairAttempts(2); // bound the loop for test latency

    // Track retry progress events.
    QObject::connect(&orch, &ai::AiOrchestrator::retryProgress,
                     [&](int attempt, int total, const QString &e) {
                         ++g_retries;
                         qInfo().noquote() << QStringLiteral("   (retry %1/%2: %3)")
                                                  .arg(attempt).arg(total).arg(preview(e, 80));
                     });

    // Probe availability, waiting for the async /api/tags reply.
    bool avail = false;
    {
        QEventLoop loop;
        QObject::connect(orch.ollama(), &ai::OllamaClient::availabilityChecked,
                         [&](bool ok, const QStringList &models) {
                             avail = ok;
                             qInfo().noquote() << "Ollama available:" << ok
                                               << "| models:" << models.join(QStringLiteral(", "));
                             loop.quit();
                         });
        QTimer t; t.setSingleShot(true);
        QObject::connect(&t, &QTimer::timeout, &loop, &QEventLoop::quit);
        t.start(5000);
        orch.ollama()->probe();
        loop.exec();
    }
    if (!avail) {
        qWarning() << "Ollama not reachable on localhost:11434 — aborting live test.";
        db.closeDatabase();
        return 3;
    }

    // ---- Q1: complex multi-subquery (verify off) -> chart, no repair needed
    {
        orch.setVerifySpec(false);
        bool to = false;
        const QString q = QStringLiteral(
            "For each department, report the department name, its average salary, the name "
            "of its highest-paid employee, its total project spend, and the count of employees "
            "whose performance_score is above the company-wide average. Order by total project "
            "spend descending.");
        auto a = askBlocking(orch, q, to);
        qInfo().noquote() << "\n[Q1 complex] source=" << a.source << "executed=" << a.executed
                          << "attempts=" << a.attempts << "repairs=" << a.repairAttempts
                          << "\n  SQL: " << preview(a.generatedSql);
        check(!to, QStringLiteral("Q1 produced an answer (no timeout)"));
        check(a.source.contains(QStringLiteral("LLM")),
              QStringLiteral("Q1 came from the local model (not the rule engine)"));
        check(a.executed && a.data.rows.size() >= 2,
              QStringLiteral("Q1 executed with multiple rows -> chart path (rows=%1)").arg(a.data.rows.size()));
        check(!a.insight.isScalar && a.insight.plottable(),
              QStringLiteral("Q1 insight is a plottable chart (type=%1)").arg(ai::chartTypeName(a.insight.chartType)));
        // Analyst stage: the second (small) model must interpret the rows, or
        // the answer must honestly say the deterministic summary stands in.
        qInfo().noquote() << "  analyst (" << a.analystSource << "): "
                          << preview(a.analystText, 160);
        check(a.executed && !a.analystText.trimmed().isEmpty() && !a.analystSource.trimmed().isEmpty(),
              QStringLiteral("Q1 carries a non-empty analyst interpretation"));
        check(a.analystSource.startsWith(QStringLiteral("Analist ("))
                  || a.analystSource.contains(QStringLiteral("istatistik")),
              QStringLiteral("Q1 analyst source honestly labels model vs fallback"));
    }

    // ---- Q2: single aggregate + spec verification (verify on) -> KPI + note
    {
        orch.setVerifySpec(true);
        bool to = false;
        auto a = askBlocking(orch, QStringLiteral("How many employees are there in total?"), to);
        qInfo().noquote() << "\n[Q2 scalar+verify] executed=" << a.executed
                          << "attempts=" << a.attempts
                          << "\n  verification: " << preview(a.verificationNote, 200);
        check(!to && a.source.contains(QStringLiteral("LLM")),
              QStringLiteral("Q2 answered by the local model"));
        check(a.executed && a.data.rows.size() == 1 && a.insight.isScalar && !a.insight.plottable(),
              QStringLiteral("Q2 is a single-row scalar -> KPI card path"));
        check(!a.verificationNote.isEmpty(),
              QStringLiteral("Q2 ran the spec-compliance verification and reported a verdict"));
    }

    // ---- Q3: impossible (non-existent tables) -> self-repair loop engages,
    //      then either fixes it or honestly gives up with SQL + DB error.
    {
        orch.setVerifySpec(false);
        bool to = false;
        const QString q = QStringLiteral(
            "List each employee's name together with their total sales revenue and "
            "commission payout, using the sales and commissions tables, ordered by revenue.");
        auto a = askBlocking(orch, q, to);
        qInfo().noquote() << "\n[Q3 repair] source=" << a.source << "executed=" << a.executed
                          << "attempts=" << a.attempts << "repairs=" << a.repairAttempts
                          << "\n  SQL: " << preview(a.generatedSql)
                          << "\n  DB error: " << preview(a.result.errorMessage, 200);
        check(!to, QStringLiteral("Q3 produced an answer (no timeout)"));
        check(a.attempts >= 2 || g_retries >= 1,
              QStringLiteral("Q3 engaged the self-repair retry loop (attempts=%1, retries=%2)")
                  .arg(a.attempts).arg(g_retries));
        check(a.source.contains(QStringLiteral("LLM")) && !a.source.contains(QStringLiteral("Kural")),
              QStringLiteral("Q3 stays attributed to the model (no silent rule downgrade)"));
        check(!a.generatedSql.isEmpty(),
              QStringLiteral("Q3 still shows the real SQL the model produced"));
        // If it could not be repaired, the DB error must be surfaced (transparency).
        check(a.executed || !a.result.errorMessage.isEmpty(),
              QStringLiteral("Q3 either self-repaired to a working query or shows the real DB error"));
    }

    db.closeDatabase();
    qInfo().noquote() << "\n" << (failures == 0 ? QStringLiteral("E2E AI TEST: ALL PASSED")
                                                 : QStringLiteral("E2E AI TEST: %1 FAILURE(S)").arg(failures));
    return failures;
}
