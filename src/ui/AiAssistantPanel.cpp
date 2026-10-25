#include "ui/AiAssistantPanel.h"

#include <QBarCategoryAxis>
#include <QBarSeries>
#include <QBarSet>
#include <QChart>
#include <QChartView>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineSeries>
#include <QPlainTextEdit>
#include <QPieSeries>
#include <QPieSlice>
#include <QPushButton>
#include <QFrame>
#include <QSizePolicy>
#include <QSqlQueryModel>
#include <QTextBrowser>
#include <QScrollBar>
#include <QTime>
#include <QTimer>
#include <QValueAxis>
#include <QXYSeries>
#include <QEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QMap>
#include <QSettings>

#include <algorithm>
#include <functional>

#include "ui/Theme.h"

namespace ui {

class ChatBubble : public QWidget {
public:
    explicit ChatBubble(bool user, QWidget *parent = nullptr)
        : QWidget(parent), m_user(user)
    {
        auto *row = new QHBoxLayout(this);
        row->setContentsMargins(4, 6, 4, 6);

        m_avatar = new QLabel(user ? QStringLiteral("SEN") : QStringLiteral("AI"), this);
        m_avatar->setAlignment(Qt::AlignCenter);
        m_avatar->setFixedSize(32, 32);

        auto *col = new QVBoxLayout;
        col->setSpacing(4);
        m_bubble = new QFrame(this);
        m_bubble->setObjectName(user ? QStringLiteral("bubbleUser") : QStringLiteral("bubbleAi"));
        auto *inner = new QVBoxLayout(m_bubble);
        inner->setContentsMargins(12, 10, 12, 10);
        inner->setSpacing(8);
        
        m_body = new QLabel(m_bubble);
        m_body->setTextFormat(Qt::RichText);
        m_body->setWordWrap(true);
        inner->addWidget(m_body);
        
        m_sqlCard = new QLabel(m_bubble);
        m_sqlCard->setObjectName(QStringLiteral("chatSqlCard"));
        m_sqlCard->setTextFormat(Qt::PlainText);
        m_sqlCard->setWordWrap(true);
        m_sqlCard->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_sqlCard->setVisible(false);
        inner->addWidget(m_sqlCard);
        
        m_copyButton = new QPushButton(QStringLiteral("Kopyala -> Editör"), m_bubble);
        m_copyButton->setObjectName(QStringLiteral("chatCopySql"));
        m_copyButton->setCursor(Qt::PointingHandCursor);
        m_copyButton->setVisible(false);
        inner->addWidget(m_copyButton);
        
        col->addWidget(m_bubble);
        
        m_time = new QLabel(QTime::currentTime().toString(QStringLiteral("HH:mm")), this);
        m_time->setObjectName(QStringLiteral("chatTime"));
        col->addWidget(m_time, 0, user ? Qt::AlignRight : Qt::AlignLeft);
        
        m_meta = new QLabel(this);
        m_meta->setTextFormat(Qt::RichText);
        m_meta->setWordWrap(true);
        m_meta->setObjectName(QStringLiteral("chatMeta"));
        m_meta->setVisible(false);
        col->addWidget(m_meta);

        connect(m_copyButton, &QPushButton::clicked, this, [this]() {
            if (m_onCopySql)
                m_onCopySql(m_sqlCard->text());
        });

        if (user) {
            row->addStretch(1);
            row->addLayout(col);
            row->addWidget(m_avatar, 0, Qt::AlignTop);
        } else {
            row->addWidget(m_avatar, 0, Qt::AlignTop);
            row->addLayout(col, 1);
            row->addStretch(1);
        }
    }

    bool isUser() const { return m_user; }
    bool isError() const { return m_error; }
    void setOnCopySql(std::function<void(const QString &)> cb) { m_onCopySql = std::move(cb); }

    /// Full visible text of the bubble (body + SQL card + meta line) so the
    /// headless UI tests can assert on what the user actually sees.
    QString displayText() const
    {
        QString t = m_body->text();
        if (!m_sqlCard->isHidden()) t += QLatin1Char('\n') + m_sqlCard->text();
        if (!m_meta->isHidden()) t += QLatin1Char('\n') + m_meta->text();
        return t;
    }

private:
    // RichText in a QLabel does not reliably inherit the widget stylesheet's
    // `color` for HTML fragments that carry no explicit color (e.g. the <b>
    // answer title), so those bits rendered white-on-white. Wrapping the body
    // in a themed outer <div> forces every uncolored run to the right tone.
    void updateBodyColor()
    {
        const QString color = m_textColor.isEmpty()
            ? QStringLiteral("#1f2937") : m_textColor;
        m_body->setText(QStringLiteral("<div style=\"color:%1;\">%2</div>")
                            .arg(color, m_bodyHtml));
    }

public:

    void setContent(const QString &bodyHtml, const QString &sql, const QString &metaHtml)
    {
        m_bodyHtml = bodyHtml;
        updateBodyColor();
        const bool hasSql = !sql.trimmed().isEmpty();
        m_sqlCard->setVisible(hasSql);
        m_copyButton->setVisible(hasSql);
        if (hasSql)
            m_sqlCard->setText(sql.trimmed());
        m_meta->setVisible(!metaHtml.isEmpty());
        if (!metaHtml.isEmpty())
            m_meta->setText(metaHtml);
        relayout();
    }

    void applyTheme(bool dark, bool error)
    {
        m_error = error;
        const QString avatarStyle = m_user
            ? QStringLiteral("QLabel { background:%1; color:#ffffff; border-radius:16px;"
                             " font-size:10px; font-weight:700; }")
                  .arg(dark ? QStringLiteral("#3b82f6") : QStringLiteral("#2563eb"))
            : QStringLiteral("QLabel { background:%1; color:%2; border:1px solid %3;"
                             " border-radius:16px; font-size:10px; font-weight:700; }")
                  .arg(dark ? QStringLiteral("#2a2c36") : QStringLiteral("#eef2ff"),
                       dark ? QStringLiteral("#b7c3ff") : QStringLiteral("#1d4ed8"),
                       dark ? QStringLiteral("#3a3d4a") : QStringLiteral("#c7d2fe"));
        m_avatar->setStyleSheet(avatarStyle);

        QString bubbleBg, bubbleBorder;
        QString text = dark ? QStringLiteral("#e6e8ee") : QStringLiteral("#1f2937");
        if (m_user) {
            bubbleBg = dark ? QStringLiteral("#1e3a8a") : QStringLiteral("#2563eb");
            bubbleBorder = dark ? QStringLiteral("#2b4fa0") : QStringLiteral("#1d4ed8");
            text = QStringLiteral("#2d0404ff");
        } else if (dark) {
            bubbleBg = QStringLiteral("#242733");
            bubbleBorder = QStringLiteral("#323644");
        } else {
            bubbleBg = QStringLiteral("#ffffff");
            bubbleBorder = QStringLiteral("#e2e8f0");
        }
        
        QString bubbleStyle = QStringLiteral(
            "QFrame#%1 { background:%2; border:1px solid %3; border-radius:16px; }")
                                 .arg(m_bubble->objectName(), bubbleBg, bubbleBorder)
                             + QStringLiteral(" QFrame { color:%1; }").arg(text);
        if (error) {
            bubbleStyle = QStringLiteral(
                "QFrame#%1 { background:%2; border:1px solid %3; border-left:4px solid #ef4444;"
                " border-radius:16px; } QFrame { color:%4; }")
                              .arg(m_bubble->objectName(),
                                   dark ? QStringLiteral("#3a2a2c") : QStringLiteral("#1b0303ff"),
                                   dark ? QStringLiteral("#5a3a3d") : QStringLiteral("#b21919ff"), text);
        }
        m_bubble->setStyleSheet(bubbleStyle);
        m_textColor = text;
        m_body->setStyleSheet(QStringLiteral("font-size:13px; line-height:1.4;"));
        updateBodyColor();
        m_sqlCard->setStyleSheet(QStringLiteral(
            "QLabel#chatSqlCard { background:%1; color:%2; border:1px solid %3;"
            " border-radius:8px; padding:10px; font-family:'Menlo','Monospace'; font-size:12px; }")
                                      .arg(dark ? QStringLiteral("#0f172a")
                                                : QStringLiteral("#f8fafc"),
                                           dark ? QStringLiteral("#93c5fd")
                                                : QStringLiteral("#1e40af"),
                                           dark ? QStringLiteral("#1e293b")
                                                : QStringLiteral("#e2e8f0")));
        m_copyButton->setStyleSheet(QStringLiteral(
            "QPushButton { background:transparent; border:1px solid %1; border-radius:6px;"
            " padding:4px 10px; font-size:11px; font-weight:600; color:%2; } QPushButton:hover { background:%3; }")
                                          .arg(dark ? QStringLiteral("#3a3d4a")
                                                    : QStringLiteral("#cbd5e1"),
                                               dark ? QStringLiteral("#b7c3ff")
                                                    : QStringLiteral("#2563eb"),
                                               dark ? QStringLiteral("#2a2c36")
                                                    : QStringLiteral("#f1f5f9")));
        m_time->setStyleSheet(QStringLiteral("color:%1; font-size:10px; padding-left:4px;")
                                  .arg(dark ? QStringLiteral("#8a90a0")
                                            : QStringLiteral("#94a3b8")));
        m_meta->setStyleSheet(QStringLiteral("color:%1; font-size:11px; padding-left:4px;")
                                  .arg(dark ? QStringLiteral("#9aa3b2")
                                            : QStringLiteral("#64748b")));
    }

protected:
    void resizeEvent(QResizeEvent *e) override
    {
        QWidget::resizeEvent(e);
        relayout();
    }

private:
    void relayout()
    {
        const int host = parentWidget() ? parentWidget()->width() : width();
        if (host <= 40) return;
        const int avail = host - 48;
        const int cap = qMax(140, int(avail * 0.85));
        const QFontMetrics fm(m_body->font());
        const int tw = fm.horizontalAdvance(m_body->text()) + 40;
        const int min = !m_sqlCard->isHidden() ? 260 : 120;
        const int w = qBound(min, qMax(tw, min), qMin(cap, avail));
        m_bubble->setFixedWidth(w);
        m_time->setFixedWidth(w);
    }

    bool m_user;
    bool m_error = false;
    std::function<void(const QString &)> m_onCopySql;
    QLabel *m_avatar = nullptr;
    QFrame *m_bubble = nullptr;
    QLabel *m_body = nullptr;
    QLabel *m_sqlCard = nullptr;
    QPushButton *m_copyButton = nullptr;
    QLabel *m_time = nullptr;
    QLabel *m_meta = nullptr;
    QString m_bodyHtml;
    QString m_textColor;
};

namespace {

QString htmlEscape(const QString &s)
{
    QString out = s;
    out.replace(QStringLiteral("&"), QStringLiteral("&amp;"));
    out.replace(QStringLiteral("<"), QStringLiteral("&lt;"));
    out.replace(QStringLiteral(">"), QStringLiteral("&gt;"));
    out.replace(QStringLiteral("\""), QStringLiteral("&quot;"));
    return out;
}

double cellToDouble(const QString &s, bool *okOut = nullptr)
{
    const QString t = s.trimmed();
    if (t.isEmpty()) {
        if (okOut) *okOut = false;
        return 0.0;
    }
    bool ok = false;
    const double v = t.toDouble(&ok);
    if (okOut) *okOut = ok;
    return v;
}

QString kpiCardHtml(const ai::Insight &ins, bool tr)
{
    auto e = [](QString s) {
        s.replace(QStringLiteral("&"), QStringLiteral("&amp;"));
        s.replace(QStringLiteral("<"), QStringLiteral("&lt;"));
        s.replace(QStringLiteral(">"), QStringLiteral("&gt;"));
        return s;
    };
    const bool dark = Theme::current() == Theme::Mode::Dark;
    const QString muted = dark ? QStringLiteral("#9aa3b2") : QStringLiteral("#64748b");
    const QString body = dark ? QStringLiteral("#cbd5e1") : QStringLiteral("#334155");
    const QString head = dark ? QStringLiteral("#f8fafc") : QStringLiteral("#0f172a");
    const QString caption = tr ? QStringLiteral("TEKIL DEGER") : QStringLiteral("SINGLE VALUE");
    QString html = QStringLiteral(
        "<div align='center' style='padding:20px'>"
        "<div style='font-size:11px;color:%4;letter-spacing:2px;font-weight:600'>%1</div>"
        "<div style='font-size:13px;color:%5;margin-top:6px'>%2</div>"
        "<div style='font-size:46px;font-weight:800;color:%6;margin-top:8px'>%3</div>")
        .arg(caption, e(ins.kpiLabel), e(ins.kpiValue), muted, body, head);

    QString extra;
    for (const auto &kv : ins.kpiItems) {
        if (kv.first == ins.kpiLabel) continue;
        extra += QStringLiteral(
            "<tr><td align='right' style='padding:4px 12px;color:%1'>%2</td>"
            "<td align='left' style='padding:4px 12px;font-weight:700;color:%3'>%4</td></tr>")
            .arg(body, e(kv.first), head, e(kv.second));
    }
    if (!extra.isEmpty()) {
        html += QStringLiteral(
            "<table cellspacing=0 cellpadding=0 "
            "style='margin-top:16px;font-size:14px'>%1</table></div>").arg(extra);
    } else {
        html += QStringLiteral("</div>");
    }
    return html;
}

void paintChart(QChartView *view, const ai::TableData &data, const ai::Insight &ins)
{
    QChart *previous = view->chart();
    auto *chart = new QChart;
    chart->setAnimationOptions(QChart::NoAnimation);

    const bool dark = Theme::current() == Theme::Mode::Dark;
    const QColor textColor(dark ? 0xe2 : 0x0f, dark ? 0xe8 : 0x17, dark ? 0xf0 : 0x2a);
    const QColor gridColor(dark ? 0x33 : 0xe2, dark ? 0x41 : 0xe8, dark ? 0x55 : 0xf0);
    chart->setBackgroundBrush(QBrush(dark ? QColor(0x1e, 0x22, 0x30) : QColor(0xff, 0xff, 0xff)));
    chart->setTitleBrush(QBrush(textColor));
    if (chart->legend())
        chart->legend()->setLabelColor(textColor);

    const int x = ins.xColumn;
    const int y = ins.yColumn;
    const int rows = data.rows.size();

    if (x < 0 || rows == 0) {
        view->setChart(chart);
        delete previous;
        return;
    }

    // Count-mode: no valid numeric measure (e.g. only an id column) -> draw how
    // many rows fall under each X category instead of plotting the key itself.
    if (y < 0) {
        QMap<QString, int> freq;
        QStringList order;
        for (int r = 0; r < rows; ++r) {
            const QString l = data.rows.at(r).value(x);
            if (!freq.contains(l)) { freq.insert(l, 0); order << l; }
            freq[l]++;
        }
        auto *set = new QBarSet(QStringLiteral("Sat\u0131r say\u0131s\u0131"));
        for (const QString &l : order)
            *set << freq.value(l);
        auto *series = new QBarSeries;
        series->append(set);
        if (order.size() <= 30) series->setLabelsVisible(true);
        chart->addSeries(series);
        auto *axisX = new QBarCategoryAxis;
        axisX->append(order);
        axisX->setLabelsColor(textColor);
        axisX->setLabelsAngle(order.size() > 8 ? -45 : 0);
        axisX->setTitleText(data.columns.value(x));
        axisX->setTitleBrush(QBrush(textColor));
        chart->addAxis(axisX, Qt::AlignBottom);
        series->attachAxis(axisX);
        auto *axisY = new QValueAxis;
        axisY->setGridLineColor(gridColor);
        axisY->setTitleText(QStringLiteral("Sat\u0131r say\u0131s\u0131"));
        axisY->setLabelsColor(textColor);
        axisY->setTitleBrush(QBrush(textColor));
        chart->addAxis(axisY, Qt::AlignLeft);
        series->attachAxis(axisY);
        chart->setTitle(QStringLiteral("Kategori baz\u0131nda sat\u0131r da\u011F\u0131l\u0131m\u0131"));
        chart->legend()->setVisible(true);
        chart->legend()->setAlignment(Qt::AlignBottom);
        chart->setMargins(QMargins(8, 8, 12, 8));
        view->setChart(chart);
        view->setRenderHint(QPainter::Antialiasing);
        delete previous;
        return;
    }

    QStringList labels;
    QVector<double> values;
    labels.reserve(rows);
    values.reserve(rows);
    for (int r = 0; r < rows; ++r) {
        bool ok = false;
        const double v = cellToDouble(data.rows.at(r).value(y), &ok);
        if (!ok) continue;
        labels << data.rows.at(r).value(x);
        values << v;
    }

    auto styleValueAxis = [&](QValueAxis *axis) {
        axis->setLabelsColor(textColor);
        axis->setTitleBrush(QBrush(textColor));
    };

    switch (ins.chartType) {
    case ai::ChartType::Pie: {
        auto *series = new QPieSeries;
        QVector<QPair<QString, double>> items;
        items.reserve(labels.size());
        for (int r = 0; r < labels.size(); ++r)
            items.append({labels.at(r), values.at(r)});
        const int maxSlices = 12;
        if (items.size() > maxSlices) {
            std::sort(items.begin(), items.end(),
                      [](const QPair<QString, double> &a, const QPair<QString, double> &b) {
                          return a.second > b.second;
                      });
            double rest = 0.0;
            for (int i = maxSlices - 1; i < items.size(); ++i)
                rest += items.at(i).second;
            items.erase(items.begin() + maxSlices - 1, items.end());
            items.append({QStringLiteral("Diğer"), rest});
        }
        const QColor sliceLabel = dark ? QColor(0xf8, 0xfa, 0xfc) : QColor(0x0f, 0x17, 0x2a);
        for (const auto &it : items) {
            auto *slice = series->append(it.first, it.second);
            slice->setLabelVisible();
            slice->setLabelColor(sliceLabel);
        }
        chart->addSeries(series);
        chart->setTitle(QStringLiteral("%1 (Pay)").arg(data.columns.value(y)));
        break;
    }
    case ai::ChartType::Bar: {
        auto *set = new QBarSet(data.columns.value(y));
        for (double v : values) *set << v;
        auto *series = new QBarSeries;
        series->append(set);
        if (labels.size() <= 30) series->setLabelsVisible(true);
        chart->addSeries(series);
        auto *axisX = new QBarCategoryAxis;
        axisX->append(labels);
        axisX->setLabelsColor(textColor);
        axisX->setLabelsAngle(labels.size() > 8 ? -45 : 0);
        axisX->setTitleText(data.columns.value(x));
        axisX->setTitleBrush(QBrush(textColor));
        chart->addAxis(axisX, Qt::AlignBottom);
        series->attachAxis(axisX);
        auto *axisY = new QValueAxis;
        axisY->setGridLineColor(gridColor);
        axisY->setTitleText(data.columns.value(y));
        styleValueAxis(axisY);
        chart->addAxis(axisY, Qt::AlignLeft);
        series->attachAxis(axisY);
        chart->setTitle(data.columns.value(y));
        break;
    }
    case ai::ChartType::Line:
    default: {
        auto *series = new QLineSeries;
        for (int r = 0; r < labels.size(); ++r)
            series->append(r, values.at(r));
        series->setPointsVisible(labels.size() <= 40);
        chart->addSeries(series);
        auto *axisX = new QBarCategoryAxis;
        axisX->append(labels);
        axisX->setLabelsColor(textColor);
        axisX->setLabelsAngle(labels.size() > 8 ? -45 : 0);
        axisX->setTitleText(data.columns.value(x));
        axisX->setTitleBrush(QBrush(textColor));
        chart->addAxis(axisX, Qt::AlignBottom);
        series->attachAxis(axisX);
        auto *axisY = new QValueAxis;
        axisY->setGridLineColor(gridColor);
        axisY->setTitleText(data.columns.value(y));
        styleValueAxis(axisY);
        chart->addAxis(axisY, Qt::AlignLeft);
        series->attachAxis(axisY);
        chart->setTitle(QStringLiteral("%1 Trendi").arg(data.columns.value(y)));
        break;
    }
    }

    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->setMargins(QMargins(8, 8, 12, 8));
    view->setChart(chart);
    view->setRenderHint(QPainter::Antialiasing);
    delete previous;
}

} // namespace

AiAssistantPanel::AiAssistantPanel(core::DatabaseManager *db, QWidget *parent)
    : QTabWidget(parent)
{
    m_orch = new ai::AiOrchestrator(db, this);
    connect(m_orch, &ai::AiOrchestrator::answerReady, this, &AiAssistantPanel::onAnswer);
    connect(m_orch, &ai::AiOrchestrator::retryProgress, this,
            [this](int attempt, int total, const QString &err) {
                if (!m_thinking) return;
                const QString short_err = err.length() > 90 ? err.left(90) + QStringLiteral("…") : err;
                m_thinking->setText(QStringLiteral("● Model hatalı SQL'i düzeltiyor "
                                                  "(deneme %1/%2): %3")
                                        .arg(attempt)
                                        .arg(total)
                                        .arg(short_err));
                m_thinking->setVisible(true);
            });
    m_orch->setStatusCallback([this](const QString &t) {
        if (!m_thinking) return;
        if (t.isEmpty()) {
            m_thinking->setVisible(false);
            return;
        }
        m_thinking->setText(QStringLiteral("● %1").arg(t));
        m_thinking->setVisible(true);
    });

    setObjectName(QStringLiteral("aiPanel"));
    buildChatTab();
    buildAnalyticsTab();

    m_orch->ollama()->probe();
}

void AiAssistantPanel::buildChatTab()
{
    auto *page = new QWidget;
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(8);

    auto *modelRow = new QHBoxLayout;
    modelRow->addWidget(new QLabel(QStringLiteral("SQL Modeli:")));
    m_modelBox = new QComboBox;
    m_modelBox->setObjectName(QStringLiteral("aiModel"));
    m_modelBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_modelBox->addItem(m_orch->ollama()->model());
    modelRow->addWidget(m_modelBox, 1);
    
    modelRow->addWidget(new QLabel(QStringLiteral("Analist:")));
    m_analystBox = new QComboBox;
    m_analystBox->setObjectName(QStringLiteral("aiAnalystModel"));
    m_analystBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_analystBox->addItem(m_orch->analyst()->model());
    modelRow->addWidget(m_analystBox, 1);
    
    m_analystCheck = new QCheckBox(QStringLiteral("Analyst"));
    m_analystCheck->setObjectName(QStringLiteral("aiAnalystEnabled"));
    m_analystCheck->setChecked(m_orch->useAnalyst());
    modelRow->addWidget(m_analystCheck);
    lay->addLayout(modelRow);

    connect(m_orch->ollama(), &ai::OllamaClient::availabilityChecked, this,
            [this](bool available, const QStringList &models) {
                m_modelBox->blockSignals(true);
                m_modelBox->clear();
                const QString cur = m_orch->ollama()->model();
                if (available && !models.isEmpty()) {
                    m_modelBox->addItems(models);
                    auto bestModel = [](const QStringList &m) {
                        for (const QString &x : m)
                            if (x.contains(QStringLiteral("coder"), Qt::CaseInsensitive)) return x;
                        for (const QString &x : m)
                            if (x.startsWith(QStringLiteral("qwen2.5"))) return x;
                        return m.first();
                    };
                    const QString chosen = models.contains(cur) ? cur : bestModel(models);
                    m_modelBox->setCurrentText(chosen);
                    m_orch->ollama()->setModel(chosen);
                } else {
                    m_modelBox->addItem(cur);
                }
                m_modelBox->setEnabled(available && !models.isEmpty());
                m_modelBox->blockSignals(false);

                m_analystBox->blockSignals(true);
                m_analystBox->clear();
                const QString curAnalyst = m_orch->analyst()->model();
                if (available && !models.isEmpty()) {
                    m_analystBox->addItems(models);
                    QString chosen = curAnalyst;
                    if (!models.contains(chosen)) {
                        chosen = models.first();
                    }
                    m_analystBox->setCurrentText(chosen);
                    m_orch->analyst()->setModel(chosen);
                } else {
                    m_analystBox->addItem(curAnalyst);
                }
                m_analystBox->setEnabled(available && !models.isEmpty());
                m_analystBox->blockSignals(false);
            });

    connect(m_modelBox, &QComboBox::currentTextChanged, this, [this](const QString &name) {
        if (!name.isEmpty()) m_orch->ollama()->setModel(name);
    });
    connect(m_analystBox, &QComboBox::currentTextChanged, this, [this](const QString &name) {
        if (!name.isEmpty()) m_orch->analyst()->setModel(name);
    });
    connect(m_analystCheck, &QCheckBox::toggled, this, [this](bool on) {
        m_orch->setUseAnalyst(on);
    });

    m_chatScroll = new QScrollArea;
    m_chatScroll->setObjectName(QStringLiteral("aiChat"));
    m_chatScroll->setWidgetResizable(true);
    m_chatScroll->setFrameShape(QFrame::NoFrame);
    m_chatScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    auto *bubbleBar = new QWidget;
    bubbleBar->setObjectName(QStringLiteral("aiChatBody"));
    m_chatBody = bubbleBar;
    m_chatLayout = new QVBoxLayout(bubbleBar);
    m_chatLayout->setContentsMargins(8, 8, 8, 8);
    m_chatLayout->setSpacing(10);
    m_chatLayout->addStretch(1);
    m_chatScroll->setWidget(bubbleBar);
    lay->addWidget(m_chatScroll, 1);

    m_thinking = new QLabel(QStringLiteral("● AI düşünüyor…"));
    m_thinking->setObjectName(QStringLiteral("aiThinking"));
    m_thinking->setStyleSheet(QStringLiteral(
        "QLabel#aiThinking { color:#2563eb; background:#eff6ff; border:1px solid #bfdbfe;"
        " border-radius:8px; padding:6px 12px; font-size:12px; font-style:italic; font-weight:500; }"));
    m_thinking->setVisible(false);
    lay->addWidget(m_thinking);

    auto *row = new QHBoxLayout;
    row->setSpacing(8);
    m_input = new QPlainTextEdit;
    m_input->setObjectName(QStringLiteral("aiInput"));
    m_input->setPlaceholderText(QStringLiteral(
        "Tablonuzu doğal dille sorun… (ör. 'Personel performanslarını ve maaşları göster')\n"
        "Enter = Gönder, Shift+Enter = Yeni satır"));
    m_input->setFixedHeight(60);
    m_input->setMaximumHeight(120);
    row->addWidget(m_input, 1);

    m_sendButton = new QPushButton(QStringLiteral("Gönder ➤"));
    m_sendButton->setObjectName(QStringLiteral("aiSendButton"));
    m_sendButton->setMinimumWidth(90);
    m_sendButton->setCursor(Qt::PointingHandCursor);
    m_sendButton->setStyleSheet(QStringLiteral(
        "QPushButton#aiSendButton { background:#2563eb; color:#ffffff; border:none;"
        " border-radius:16px; padding:8px 16px; font-weight:600; font-size:13px; }"
        "QPushButton#aiSendButton:hover { background:#1d4ed8; }"
        "QPushButton#aiSendButton:disabled { background:#94a3b8; }"));
    row->addWidget(m_sendButton);
    lay->addLayout(row);

    connect(m_sendButton, &QPushButton::clicked, this, &AiAssistantPanel::askQuestion);
    m_input->installEventFilter(this);

    addTab(page, QStringLiteral("Chat"));
}

void AiAssistantPanel::buildAnalyticsTab()
{
    auto *page = new QWidget;
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(8);

    auto *controls = new QHBoxLayout;
    m_chartType = new QComboBox;
    m_chartType->setObjectName(QStringLiteral("aiChartType"));
    m_chartType->addItems({QStringLiteral("Auto"), QStringLiteral("Line"),
                           QStringLiteral("Bar"), QStringLiteral("Pie")});
    controls->addWidget(m_chartType);
    controls->addStretch(1);

    m_langButton = new QPushButton(QStringLiteral("TR"));
    m_langButton->setObjectName(QStringLiteral("aiLangButton"));
    m_langButton->setCheckable(true);
    controls->addWidget(m_langButton);
    lay->addLayout(controls);

    auto *axisRow = new QHBoxLayout;
    axisRow->addWidget(new QLabel(QStringLiteral("X:")));
    m_xCol = new QComboBox;
    m_xCol->setObjectName(QStringLiteral("aiXCol"));
    m_xCol->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_xCol->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_xCol->setMinimumContentsLength(12);
    axisRow->addWidget(m_xCol, 1);

    axisRow->addWidget(new QLabel(QStringLiteral("Y:")));
    m_yCol = new QComboBox;
    m_yCol->setObjectName(QStringLiteral("aiYCol"));
    m_yCol->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_yCol->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_yCol->setMinimumContentsLength(12);
    axisRow->addWidget(m_yCol, 1);
    lay->addLayout(axisRow);

    m_chartView = new QChartView(new QChart);
    m_chartView->setObjectName(QStringLiteral("aiChartView"));
    m_chartView->setRenderHint(QPainter::Antialiasing);
    lay->addWidget(m_chartView, 1);

    m_kpiCard = new QLabel;
    m_kpiCard->setObjectName(QStringLiteral("aiKpiCard"));
    m_kpiCard->setTextFormat(Qt::RichText);
    m_kpiCard->setAlignment(Qt::AlignCenter);
    m_kpiCard->setWordWrap(true);
    m_kpiCard->setStyleSheet(QStringLiteral(
        "QLabel { background:#ffffff; border:1px solid #e2e8f0; border-radius:14px; }"));
    m_kpiCard->setVisible(false);
    lay->addWidget(m_kpiCard, 1);

    m_summary = new QLabel;
    m_summary->setObjectName(QStringLiteral("aiSummary"));
    m_summary->setWordWrap(true);
    m_summary->setTextFormat(Qt::RichText);
    m_summary->setMaximumHeight(90);
    lay->addWidget(m_summary);

    m_report = new QTextBrowser;
    m_report->setObjectName(QStringLiteral("aiReport"));
    m_report->setOpenExternalLinks(false);
    lay->addWidget(m_report, 1);

    connect(m_chartType, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_hasData) renderChart(m_lastData, m_lastInsight);
    });
    connect(m_xCol, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx < 0 || !m_hasData) return;
        renderChart(m_lastData, m_lastInsight);
    });
    connect(m_yCol, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx < 0 || !m_hasData) return;
        renderChart(m_lastData, m_lastInsight);
    });
    connect(m_langButton, &QPushButton::toggled, this, [this](bool en) {
        m_langTr = !en;
        if (m_hasData) rebuildAnalytics();
    });

    addTab(page, QStringLiteral("Analytics"));
}

bool AiAssistantPanel::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_input && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (!(keyEvent->modifiers() & Qt::ShiftModifier)) {
                askQuestion();
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void AiAssistantPanel::askQuestion()
{
    const QString q = m_input->toPlainText().trimmed();
    if (q.isEmpty()) return;
    m_input->clear();

    appendMessage(QStringLiteral("user"), htmlEscape(q), QString(), QString(), false);
    m_sendButton->setEnabled(false);
    setThinking(true);

    m_orch->ask(q);
}

void AiAssistantPanel::appendMessage(const QString &who, const QString &bodyHtml,
                                     const QString &sql, const QString &metaHtml,
                                     bool isError)
{
    const bool user = (who == QLatin1String("user"));
    const bool dark = Theme::current() == Theme::Mode::Dark;
    auto *bubble = new ChatBubble(user, m_chatBody);
    bubble->applyTheme(dark, isError);
    if (!user) {
        bubble->setOnCopySql([this](const QString &sqlText) {
            emit insertSqlRequested(sqlText);
        });
    }
    bubble->setContent(bodyHtml, sql, metaHtml);
    m_bubbles.append(bubble);
    m_chatLayout->insertWidget(m_chatLayout->count() - 1, bubble);
    scrollChatToBottom();
}

void AiAssistantPanel::scrollChatToBottom()
{
    QTimer::singleShot(50, this, [this]() {
        if (QScrollBar *bar = m_chatScroll->verticalScrollBar())
            bar->setValue(bar->maximum());
    });
}

void AiAssistantPanel::setThinking(bool on)
{
    if (m_thinking) m_thinking->setVisible(on);
}

void AiAssistantPanel::onAnswer(const ai::AiOrchestrator::Answer &answer)
{
    m_sendButton->setEnabled(true);
    setThinking(false);

    // Pure small talk (greeting/thanks/help): show a friendly prose bubble and
    // stop - never touch the analytics tab or emit a result-present request.
    if (answer.conversational) {
        appendMessage(QStringLiteral("ai"), htmlEscape(answer.message), QString(), QString(), false);
        return;
    }

    const bool isError = !answer.executed || !answer.result.errorMessage.isEmpty();

    QString bodyHtml;
    if (isError) {
        const QString err = answer.result.errorMessage.isEmpty()
                                ? answer.message
                                : answer.result.errorMessage;
        bodyHtml = QStringLiteral("<b>Hata:</b> %1").arg(htmlEscape(err));
    } else {
        QString text = answer.insight.summaryTr;
        if (text.isEmpty()) text = answer.message;
        bodyHtml = htmlEscape(text);
        if (!answer.analystText.isEmpty()) {
            bodyHtml += QStringLiteral("<div style='margin-top:8px'>"
                                       "<b>Analist Yorumu:</b> %1</div>")
                            .arg(htmlEscape(answer.analystText));
        }
        if (!answer.verificationNote.isEmpty()) {
            bodyHtml += QStringLiteral("<div style='margin-top:4px'>%1</div>")
                            .arg(htmlEscape(answer.verificationNote));
        }
    }

    QString meta;
    if (!answer.source.isEmpty()) meta = answer.source;
    if (!isError && !answer.data.rows.isEmpty()) {
        const QString size = QStringLiteral("%1 sat\u0131r, %2 s\u00FCtun")
                                 .arg(answer.data.rows.size())
                                 .arg(answer.data.columns.size());
        meta = meta.isEmpty() ? size : meta + QStringLiteral(" \u00b7 ") + size;
    }

    appendMessage(QStringLiteral("ai"), bodyHtml, answer.generatedSql, meta, isError);

    if (!isError) {
        m_analystText = answer.analystText;
        m_analystSource = answer.analystSource;
        loadDataset(answer.data, answer.insight);
        rebuildAnalytics();
        setCurrentIndex(1);
        emit presentResult(answer.generatedSql, answer.data, answer.source, answer.message);
    }
}

void AiAssistantPanel::loadDataset(const ai::TableData &data, const ai::Insight &insight)
{
    m_lastData = data;
    m_lastInsight = insight;
    m_hasData = !data.rows.isEmpty();

    m_xCol->blockSignals(true);
    m_yCol->blockSignals(true);
    m_xCol->clear();
    m_yCol->clear();
    m_xCol->addItems(data.columns);
    m_yCol->addItems(data.columns);
    if (insight.xColumn >= 0 && insight.xColumn < data.columns.size())
        m_xCol->setCurrentIndex(insight.xColumn);
    if (insight.yColumn >= 0 && insight.yColumn < data.columns.size())
        m_yCol->setCurrentIndex(insight.yColumn);
    m_xCol->blockSignals(false);
    m_yCol->blockSignals(false);
}

void AiAssistantPanel::renderChart(const ai::TableData &data, const ai::Insight &insight)
{
    ai::Insight ins = insight;
    const int sel = m_chartType ? m_chartType->currentIndex() : 0;
    if (sel == 1) ins.chartType = ai::ChartType::Line;
    else if (sel == 2) ins.chartType = ai::ChartType::Bar;
    else if (sel == 3) ins.chartType = ai::ChartType::Pie;
    if (m_xCol && m_xCol->currentIndex() >= 0) ins.xColumn = m_xCol->currentIndex();

    // Only a REAL numeric metric may be measured on the Y axis. An id/serial
    // column (or a non-numeric pick) forces count-mode, so surrogate keys like
    // `id` are never summed or treated as a measure.
    const int yIdx = m_yCol && m_yCol->currentIndex() >= 0 ? m_yCol->currentIndex() : insight.yColumn;
    if (yIdx >= 0 && yIdx < data.columns.size()
        && !ai::DataInspector::columnLooksIdentifier(data, yIdx)
        && ai::DataInspector::columnLooksNumeric(data, yIdx)) {
        ins.yColumn = yIdx;
    } else {
        ins.yColumn = -1; // count-mode
    }

    if (ins.isScalar || !ins.kpiLabel.isEmpty()) {
        m_kpiCard->setText(kpiCardHtml(ins, !(m_langButton && m_langButton->isChecked())));
        m_kpiCard->setVisible(true);
        m_chartView->setVisible(false);
    } else {
        m_kpiCard->setVisible(false);
        m_chartView->setVisible(true);
        paintChart(m_chartView, data, ins);
    }
}

void AiAssistantPanel::rebuildAnalytics()
{
    if (!m_hasData) {
        m_chartView->setVisible(false);
        m_kpiCard->setVisible(false);
        m_summary->clear();
        m_report->clear();
        return;
    }

    const bool en = m_langButton && m_langButton->isChecked();
    QString summary = en ? m_lastInsight.summaryEn : m_lastInsight.summaryTr;
    if (!m_analystText.isEmpty()) summary = m_analystText;
    m_summary->setText(summary);

    renderChart(m_lastData, m_lastInsight);

    const QString mgr = QStringLiteral(
        "<div style='padding:10px 12px;background:%1;border:1px solid %2;"
        "border-radius:10px;margin-bottom:10px'>"
        "<div style='font-size:12px;font-weight:700;color:#1d4ed8;"
        "letter-spacing:1px'>YONETICI OZETI</div>"
        "<div style='margin-top:4px;font-size:13px;color:%3'>%4</div></div>")
        .arg(QStringLiteral("#eff6ff"), QStringLiteral("#bfdbfe"),
             en ? QStringLiteral("#e6e8ee") : QStringLiteral("#101828"),
             htmlEscape(summary.isEmpty() ? QStringLiteral("Sonuc hesaplandi.") : summary));
    m_report->setHtml(mgr + (en ? m_lastInsight.reportHtmlEn : m_lastInsight.reportHtmlTr));
}

void AiAssistantPanel::showResultFromQuery(const core::QueryResult &result)
{
    const ai::TableData td = ai::TableData::fromQueryResult(result);
    if (td.rows.isEmpty()) return;
    m_analystText.clear();
    m_analystSource.clear();
    loadDataset(td, ai::DataInspector::analyze(td));
    rebuildAnalytics();
}

void AiAssistantPanel::showResultFromModel(QSqlQueryModel *model)
{
    if (!model) return;
    ai::TableData td;
    const int cols = model->columnCount();
    for (int c = 0; c < cols; ++c)
        td.columns << model->headerData(c, Qt::Horizontal).toString();
    for (int r = 0; r < model->rowCount(); ++r) {
        QVector<QString> row;
        row.reserve(cols);
        for (int c = 0; c < cols; ++c)
            row << model->index(r, c).data(Qt::DisplayRole).toString();
        td.rows << row;
    }
    if (td.rows.isEmpty()) return;
    m_analystText.clear();
    m_analystSource.clear();
    if (m_chartType) m_chartType->setCurrentIndex(0);
    loadDataset(td, ai::DataInspector::analyze(td));
    rebuildAnalytics();
}

int AiAssistantPanel::messageCount() const
{
    return m_bubbles.size();
}

QString AiAssistantPanel::lastAiBubbleText() const
{
    for (int i = m_bubbles.size() - 1; i >= 0; --i)
        if (!m_bubbles.at(i)->isUser())
            return m_bubbles.at(i)->displayText();
    return QString();
}

QString AiAssistantPanel::lastAnalystText() const
{
    return m_analystText;
}

int AiAssistantPanel::chartSeriesCount() const
{
    if (!m_chartView || !m_chartView->chart()) return 0;
    return m_chartView->chart()->series().count();
}

int AiAssistantPanel::chartPointCount() const
{
    if (!m_chartView || !m_chartView->chart()) return 0;
    int total = 0;
    const auto list = m_chartView->chart()->series();
    for (QAbstractSeries *s : list) {
        if (auto *xy = qobject_cast<QXYSeries *>(s))
            total += xy->count();
        else if (auto *bar = qobject_cast<QBarSeries *>(s)) {
            for (QBarSet *bs : bar->barSets()) total += bs->count();
        } else if (auto *pie = qobject_cast<QPieSeries *>(s))
            total += pie->count();
    }
    return total;
}

void AiAssistantPanel::refreshTheme()
{
    const bool dark = Theme::current() == Theme::Mode::Dark;

    for (ChatBubble *b : m_bubbles)
        b->applyTheme(dark, b->isError());

    const QString kpiBg = dark ? QStringLiteral("#1e293b") : QStringLiteral("#ffffff");
    const QString kpiBorder = dark ? QStringLiteral("#334155") : QStringLiteral("#e2e8f0");
    m_kpiCard->setStyleSheet(QStringLiteral(
        "QLabel { background:%1; border:1px solid %2; border-radius:14px; }").arg(kpiBg, kpiBorder));

    if (m_hasData) rebuildAnalytics();
}

} // namespace ui