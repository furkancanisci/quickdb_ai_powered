#pragma once

#include <QObject>

#include <functional>

#include "ai/DataInspector.h"
#include "ai/OllamaClient.h"
#include "ai/RuleBasedNlu.h"
#include "ai/SchemaContext.h"
#include "core/DatabaseManager.h"

namespace ai {

/// Runs the full question pipeline:
///   question -> (local LLM via Ollama, else rule engine) -> read-only SQL
///            -> DatabaseManager execution -> TableData + Insight.
/// When the model's SQL fails to run, the orchestrator can FEED THE DATABASE
/// ERROR back to the model and retry (bounded self-repair loop). Once a query
/// finally succeeds it can optionally run a spec-compliance self-review.
/// A second, smaller "business analyst" model (m_analyst, default
/// qwen2.5:1.5b) then turns the raw rows into an interpreted executive summary
/// (Answer::analystText); if it cannot, the deterministic Insight text is used
/// and the answer says so honestly. Emits answerReady() exactly once per ask().
class AiOrchestrator : public QObject {
    Q_OBJECT

public:
    struct Answer {
        QString question;
        QString generatedSql;
        QString source;    // "LLM (Ollama)" | "Kural Motoru" | "Sohbet" | ""
        QString message;   // model/nlu explanation or failure reason
        bool executed = false;
        /// True for pure small talk (greeting/thanks/help): answered in prose,
        /// never turned into SQL and never sent to the analyst stage.
        bool conversational = false;
        core::QueryResult result;      // success + hasResultSet when executed
        TableData data;                // materialized result rows
        Insight insight;               // chart decision + TR/EN summaries
        int attempts = 1;              // total model generations used
        int repairAttempts = 0;        // how many self-repair retries ran
        QString verificationNote;      // spec-compliance verdict ("" if not run)
        QString analystText;           // executive interpretation of the data
        QString analystSource;         // "Analist (model)" | reason for fallback
    };

    explicit AiOrchestrator(core::DatabaseManager *db, QObject *parent = nullptr);

    OllamaClient *ollama() { return m_ollama; }
    /// Second client used only for the analyst step (own model + availability).
    OllamaClient *analyst() { return m_analyst; }
    void setUseLlm(bool on) { m_useLlm = on; }
    bool useLlm() const { return m_useLlm; }

    /// Analyst stage on/off (persisted by the UI under "analystEnabled").
    void setUseAnalyst(bool on) { m_useAnalyst = on; }
    bool useAnalyst() const { return m_useAnalyst; }

    /// Lets the UI show what the pipeline is currently doing ("Analist modeli
    /// yorumluyor…") without adding another progress signal.
    void setStatusCallback(std::function<void(const QString &)> cb)
    {
        m_statusCb = std::move(cb);
    }

    // --- Self-repair / verification knobs ---------------------------------
    void setSelfRepair(bool on) { m_selfRepair = on; }
    bool selfRepair() const { return m_selfRepair; }
    void setVerifySpec(bool on) { m_verifySpec = on; }
    bool verifySpec() const { return m_verifySpec; }
    /// Max number of REPAIR retries after the first generation (default 4,
    /// i.e. up to 5 model generations total).
    void setMaxRepairAttempts(int n) { m_maxRepair = qMax(0, n); }
    int maxRepairAttempts() const { return m_maxRepair; }

    /// Starts the pipeline for one natural-language question.
    void ask(const QString &question);

    /// Re-runs the last generated SQL (e.g. user edited nothing, just wants
    /// fresh data). Returns false when there is nothing to re-run.
    bool reRunLast();

signals:
    void answerReady(const ai::AiOrchestrator::Answer &answer);
    /// Emitted for each self-repair retry so the UI can show progress.
    void retryProgress(int attempt, int totalAllowed, const QString &errorPreview);

private:
    enum Phase { Idle, AwaitingSql, AwaitingVerify, AwaitingAnalyst };

    void onGenerated(bool ok, const QString &raw, const QString &error);
    void executeSql(const QString &sql);           // runs + may trigger a repair
    void requestRepair(const QString &failedSql, const QString &error);
    void requestVerification(const Answer &successful);
    void fallbackToRules(const QString &question);
    void runSql(const QString &question, const QString &sql, const QString &source,
                const QString &message);
    Answer finish(const QString &question, const QString &sql, const QString &source,
                  const QString &message, const core::QueryResult &result);

    /// Final gate: runs the analyst stage when possible, else emits directly.
    void dispatchAnswer(const Answer &a);
    void runAnalyst(const Answer &a);
    void onAnalystFinished(bool ok, const QString &raw, const QString &error);
    void applyAnalystFallback(Answer &a, const QString &reason) const;

    QString sqlPrompt(const QString &question);
    /// "SQLite" or a PostgreSQL hint, based on the live connection's driver.
    QString dialectHint() const;
    QString repairPrompt(const QString &question, const QString &failedSql, const QString &error);
    QString verifyPrompt(const Answer &a);
    QString analystPrompt(const Answer &a) const;

    core::DatabaseManager *m_db;
    OllamaClient *m_ollama;
    OllamaClient *m_analyst = nullptr;
    bool m_useLlm = true;
    bool m_selfRepair = true;
    bool m_verifySpec = true;
    bool m_useAnalyst = true;
    int m_maxRepair = 4;
    std::function<void(const QString &)> m_statusCb;

    QString m_lastSql;
    QString m_pendingQuestion; // question currently being processed
    Phase m_phase = Idle;
    int m_attempt = 0;         // generations used for the current question
    int m_repairsDone = 0;     // repair retries used for the current question
    Answer m_stash;            // successful answer awaiting verification
};

} // namespace ai

Q_DECLARE_METATYPE(ai::AiOrchestrator::Answer)
