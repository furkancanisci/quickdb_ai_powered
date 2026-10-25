#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QString>
#include <QStringList>

namespace ai {

/// Talks to the local Ollama server (http://localhost:11434) over the LAN-free
/// loopback interface. Nothing ever leaves the machine; when the server is
/// down, available() turns false and the orchestrator uses RuleBasedNlu.
class OllamaClient : public QObject {
    Q_OBJECT

public:
    explicit OllamaClient(QObject *parent = nullptr);

    void setModel(const QString &model) { m_model = model; }
    QString model() const { return m_model; }

    /// Names of the models the local Ollama server reported during the last
    /// successful probe (empty until `probe()` completes). Lets the UI offer a
    /// model picker so a stronger (larger) model can be chosen at runtime.
    QStringList installedModels() const { return m_models; }

    /// Async probe of GET /api/tags; result delivered via availabilityChecked().
    /// Also updates the cached flag so callers can cheaply check isAvailable().
    void probe();
    bool isAvailable() const { return m_available; }

    /// Sends the prompt to POST /api/generate (stream:false) using the
    /// SQL-generator system prompt. Emits generationFinished with the raw
    /// model text on success.
    void generateSql(const QString &prompt);

    /// Generic single-turn generation with a caller-supplied system prompt.
    /// Used for the spec-compliance self-review (which must answer in prose,
    /// not SQL). Same generationFinished signal is emitted.
    void generate(const QString &system, const QString &prompt);

signals:
    void availabilityChecked(bool available, const QStringList &installedModels);
    void generationFinished(bool ok, const QString &rawText, const QString &error);

private:
    QNetworkAccessManager *m_nam;
    // Strongest practical on-device SQL model that fits under a ~2 GB budget.
    QString m_model = QStringLiteral("qwen2.5-coder:3b");
    QStringList m_models;
    bool m_available = false;
};

} // namespace ai
