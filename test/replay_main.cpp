// One-shot replay driver: sends an arbitrary question file through the live
// orchestrator (same pipeline as the GUI, with self-repair + verification).
// Usage: e2e_replay <db.path> <questionFile>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QTextStream>
#include <QTimer>

#include "ai/AiOrchestrator.h"
#include "core/DatabaseManager.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() < 3) {
        qWarning() << "usage: e2e_replay <db> <questionFile>";
        return 2;
    }
    QFile qf(args.at(2));
    if (!qf.open(QIODevice::ReadOnly | QIODevice::Text))
        return 2;
    const QString question = QTextStream(&qf).readAll().trimmed();

    core::DatabaseManager db;
    QString err;
    if (!db.openDatabase(args.at(1), &err)) {
        qWarning().noquote() << "cannot open DB:" << err;
        return 2;
    }
    ai::AiOrchestrator orch(&db);
    orch.setUseLlm(true);
    orch.setSelfRepair(true);
    orch.setMaxRepairAttempts(4);
    orch.setVerifySpec(true);

    QObject::connect(&orch, &ai::AiOrchestrator::retryProgress,
                     [](int attempt, int total, const QString &e) {
                         qInfo().noquote() << QStringLiteral("   (repair denemesi %1/%2: %3)")
                                                  .arg(attempt).arg(total)
                                                  .arg(e.left(140).replace(QLatin1Char('\n'), QLatin1Char(' ')));
                     });

    bool avail = false;
    {
        QEventLoop loop;
        QObject::connect(orch.ollama(), &ai::OllamaClient::availabilityChecked,
                         [&](bool ok, const QStringList &) { avail = ok; loop.quit(); });
        QTimer t; t.setSingleShot(true);
        QObject::connect(&t, &QTimer::timeout, &loop, &QEventLoop::quit);
        t.start(5000);
        orch.ollama()->probe();
        loop.exec();
    }
    if (!avail) {
        qWarning() << "Ollama not reachable.";
        return 3;
    }

    ai::AiOrchestrator::Answer got;
    bool done = false;
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
    watchdog.start(900000); // 15 min budget for a long repair chain
    orch.ask(question);
    loop.exec();
    QObject::disconnect(conn);

    if (!done) {
        qWarning() << "TIMEOUT waiting for the final answer.";
        return 4;
    }
    qInfo().noquote() << "\n===== FINAL ANSWER =====";
    qInfo().noquote() << "source:   " << got.source;
    qInfo().noquote() << "executed: " << got.executed;
    qInfo().noquote() << QString::fromUtf8("deneme:   %1 (duzeltme: %2)").arg(got.attempts).arg(got.repairAttempts);
    qInfo().noquote() << QString::fromUtf8("satir sayisi (SQL): %1").arg(got.generatedSql.count(QLatin1Char('\n')) + 1);
    qInfo().noquote() << "message:  " << got.message;
    if (!got.result.errorMessage.isEmpty())
        qInfo().noquote() << "db hata:  " << got.result.errorMessage;
    if (!got.verificationNote.isEmpty())
        qInfo().noquote() << "dogrulama:" << got.verificationNote;
    if (got.executed) {
        qInfo().noquote() << QString::fromUtf8("satirlar: %1 | kolonlar: %2")
                                 .arg(got.data.rows.size())
                                 .arg(got.data.columns.join(QStringLiteral(", ")));
        if (!got.data.rows.isEmpty()) {
            QString first;
            for (int c = 0; c < got.data.columns.size() && c < 6; ++c)
                first += QStringLiteral("%1=%2; ").arg(got.data.columns.at(c), got.data.rows.at(0).value(c));
            qInfo().noquote() << "ilk satir (ozet): " << first;
        }
    }
    db.closeDatabase();
    return 0;
}
