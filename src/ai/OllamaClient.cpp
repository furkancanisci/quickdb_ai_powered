#include "ai/OllamaClient.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace ai {

namespace {
constexpr const char *kBaseUrl = "http://localhost:11434";
constexpr int kProbeTimeoutMs = 2500;
// Larger models producing long, fully-expanded SQL need generous limits.
constexpr int kGenerateTimeoutMs = 180000;

// The SQL-generator persona used by generateSql(). Kept here so generate()
// can reuse it while the self-review path supplies its own prose system prompt.
const char *kSqlSystemPrompt =
    "You are a SQLite SQL generator. Given a schema and a question, reply with "
    "exactly one read-only SQL statement inside a ```sql code fence. "
    "Write the complete query without truncation, ellipsis or comments such as "
    "'...'; expand every requested subquery fully even if it becomes long. "
    "No prose/explanations outside the fence, and never "
    "INSERT/UPDATE/DELETE/DROP/ALTER/CREATE or more than one statement.";

/// Single-shot timeout watchdog for a reply.
void attachTimeout(QNetworkReply *reply, int ms)
{
    auto *timer = new QTimer(reply);
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, reply, [reply] { reply->abort(); });
    timer->start(ms);
}
} // namespace

OllamaClient::OllamaClient(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

void OllamaClient::probe()
{
    QNetworkRequest req(QUrl(QLatin1String(kBaseUrl) + QStringLiteral("/api/tags")));
    QNetworkReply *reply = m_nam->get(req);
    attachTimeout(reply, kProbeTimeoutMs);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        QStringList models;
        m_available = false;
        if (reply->error() == QNetworkReply::NoError) {
            const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            const QJsonArray arr = doc.object().value(QStringLiteral("models")).toArray();
            for (const QJsonValue &v : arr)
                models << v.toObject().value(QStringLiteral("name")).toString();
            m_available = true;
            m_models = models;
        }
        reply->deleteLater();
        emit availabilityChecked(m_available, models);
    });
}

void OllamaClient::generateSql(const QString &prompt)
{
    generate(QLatin1String(kSqlSystemPrompt), prompt);
}

void OllamaClient::generate(const QString &system, const QString &prompt)
{
    if (!m_available) {
        emit generationFinished(false, QString(),
                                QStringLiteral("Ollama servisi calismiyor (localhost:11434)."));
        return;
    }

    QJsonObject body;
    body.insert(QStringLiteral("model"), m_model);
    body.insert(QStringLiteral("prompt"), prompt);
    body.insert(QStringLiteral("stream"), false);
    body.insert(QStringLiteral("system"), system);

    // Allow long multi-subquery answers and low randomness for valid SQL.
    QJsonObject options;
    options.insert(QStringLiteral("temperature"), 0.1);
    options.insert(QStringLiteral("num_predict"), 8192);
    body.insert(QStringLiteral("options"), options);

    QNetworkRequest req(QUrl(QLatin1String(kBaseUrl) + QStringLiteral("/api/generate")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QNetworkReply *reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    attachTimeout(reply, kGenerateTimeoutMs);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const QString err = reply->errorString();
        const QByteArray data = reply->readAll();
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit generationFinished(false, QString(), err);
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        const QString text = doc.object().value(QStringLiteral("response")).toString();
        if (text.trimmed().isEmpty()) {
            emit generationFinished(false, QString(), QStringLiteral("Model bos yanit dondurdu."));
            return;
        }
        emit generationFinished(true, text, QString());
    });
}

} // namespace ai
