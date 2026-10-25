#pragma once

#include <QTabWidget>
#include <QVector>

#include "ai/AiOrchestrator.h"

class QChartView;
class QCheckBox;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QSqlQueryModel;
class QTextBrowser;
class QVBoxLayout;

namespace ui {

class ChatBubble; // widget-based message row (defined in the .cpp)

/// Right-hand AI dock: a Chat tab (natural language -> SQL -> answer bubbles)
/// and an Analytics tab (QtCharts view + TR/EN executive summary). Owns an
/// ai::AiOrchestrator that runs everything through the shared DatabaseManager.
class AiAssistantPanel : public QTabWidget {
    Q_OBJECT

public:
    explicit AiAssistantPanel(core::DatabaseManager *db, QWidget *parent = nullptr);

    ai::AiOrchestrator *orchestrator() { return m_orch; }

    /// Feeds a result the user already produced in the main grid into the
    /// Analytics tab without asking a question (chart + summary only).
    void showResultFromQuery(const core::QueryResult &result);

    /// Same, but sourced from the read-only result model (manual Execute).
    /// Does NOT switch tabs, only refreshes the Analytics chart/summary.
    void showResultFromModel(QSqlQueryModel *model);

    /// Re-renders theme-sensitive UI (chart palette, chat bubbles) after a
    /// Theme::apply() mode switch.
    void refreshTheme();

    // Introspection for the headless UI tests.
    int chartSeriesCount() const;
    int chartPointCount() const;
    int messageCount() const;
    QString lastAiBubbleText() const;
    /// Analyst-stage interpretation of the last executed answer ("" before
    /// the first successful answer).
    QString lastAnalystText() const;

public slots:
    void askQuestion();          // sends the chat input to the orchestrator
    void onAnswer(const ai::AiOrchestrator::Answer &answer);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    /// Emitted when the user clicks a bubble's SQL so the main window can
    /// load it into the editor / grid.
    void insertSqlRequested(const QString &sql);
    /// Emitted after a successful AI answer so the main window can drop the
    /// SQL into the editor and show the SAME result (no re-query) in a grid tab.
    void presentResult(const QString &sql, const ai::TableData &data,
                       const QString &source, const QString &message);

private:
    void buildChatTab();
    void buildAnalyticsTab();
    /// Appends a real widget bubble (avatar chip + rounded card + timestamp).
    /// Optional `sql` renders as a monospace code card with its own
    /// "copy to editor" button; `metaHtml` is the muted status line.
    void appendMessage(const QString &who, const QString &bodyHtml,
                       const QString &sql = QString(),
                       const QString &metaHtml = QString(),
                       bool isError = false);
    void scrollChatToBottom();
    void renderChart(const ai::TableData &data, const ai::Insight &insight);
    void rebuildAnalytics();
    void loadDataset(const ai::TableData &data, const ai::Insight &insight);
    void setThinking(bool on);

    ai::AiOrchestrator *m_orch = nullptr;

    // Chat tab (widget bubbles inside a scroll area).
    QScrollArea *m_chatScroll = nullptr;
    QWidget *m_chatBody = nullptr;
    QVBoxLayout *m_chatLayout = nullptr;
    QVector<ChatBubble *> m_bubbles;
    QPlainTextEdit *m_input = nullptr;
    QPushButton *m_sendButton = nullptr;
    QLabel *m_thinking = nullptr;
    QComboBox *m_modelBox = nullptr;  // local model picker (installed Ollama models)
    QComboBox *m_analystBox = nullptr; // second (business analyst) model picker
    QCheckBox *m_analystCheck = nullptr; // analyst stage on/off ("analystEnabled")

    // Analytics tab
    QChartView *m_chartView = nullptr;
    QLabel *m_kpiCard = nullptr;      // big-typography KPI card for scalar results
    QComboBox *m_chartType = nullptr;
    QComboBox *m_xCol = nullptr;
    QComboBox *m_yCol = nullptr;
    QLabel *m_summary = nullptr;
    QTextBrowser *m_report = nullptr;
    QPushButton *m_langButton = nullptr;
    bool m_langTr = true;

    ai::TableData m_lastData;
    ai::Insight m_lastInsight;
    bool m_hasData = false;
    QString m_analystText;   // latest Answer::analystText (exec summary card)
    QString m_analystSource; // who/why produced (or replaced) that text
};

} // namespace ui
