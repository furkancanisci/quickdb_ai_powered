#include "ai/DataInspector.h"

#include "ai/SchemaContext.h"

#include <QRegularExpression>
#include <QSqlRecord>
#include <QMap>
#include <QPair>
#include <QSet>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace ai {

namespace {

double toDouble(const QString &s, bool *ok = nullptr)
{
    double v = 0.0;
    bool parsed = false;
    if (!s.trimmed().isEmpty()) {
        v = s.toDouble(&parsed);
    }
    if (ok)
        *ok = parsed;
    return v;
}

QString fmt(double v)
{
    // Group with thousands separator when large; trim trailing zeros.
    QString s = QString::number(v, 'f', std::fabs(v - std::round(v)) < 1e-9 ? 0 : 2);
    if (std::fabs(v) >= 1000.0) {
        const QString whole = QString::number(static_cast<qint64>(std::llround(v)));
        s = whole;
        s.replace(QRegularExpression(QStringLiteral("\\B(?=(\\d{3})+(?!\\d))")),
                  QStringLiteral("."));
    }
    return s;
}

const QRegularExpression &isoDateRe()
{
    static const QRegularExpression re(QStringLiteral("^\\d{4}[-/]\\d{1,2}([-/]\\d{1,2})?"));
    return re;
}

QString esc(const QString &s)
{
    QString out = s;
    out.replace(QStringLiteral("&"), QStringLiteral("&amp;"));
    out.replace(QStringLiteral("<"), QStringLiteral("&lt;"));
    out.replace(QStringLiteral(">"), QStringLiteral("&gt;"));
    return out;
}

/// Linear-interpolated quantile over an ascending-sorted sample.
double quantile(QVector<double> sorted, double p)
{
    const int n = sorted.size();
    if (n == 0)
        return 0.0;
    std::sort(sorted.begin(), sorted.end());
    if (n == 1)
        return sorted.at(0);
    const double idx = p * (n - 1);
    const int lo = static_cast<int>(std::floor(idx));
    const int hi = static_cast<int>(std::ceil(idx));
    if (lo == hi)
        return sorted.at(lo);
    const double frac = idx - lo;
    return sorted.at(lo) * (1.0 - frac) + sorted.at(hi) * frac;
}

/// One aggregated contribution bucket for the distribution section.
struct Bucket {
    QString label;
    double value = 0.0;
    double share = 0.0; // percentage of total
};

} // namespace

TableData TableData::fromQueryResult(const core::QueryResult &result)
{
    TableData data;
    if (!result.success || !result.hasResultSet)
        return data;

    QSqlQuery q = result.query; // copy-on-write handle; source moved here
    const QSqlRecord record = q.record();
    for (int i = 0; i < record.count(); ++i)
        data.columns << record.fieldName(i);
    while (q.next()) {
        QVector<QString> row;
        for (int i = 0; i < record.count(); ++i)
            row << q.value(i).toString();
        data.rows.append(row);
    }
    return data;
}

bool DataInspector::columnLooksNumeric(const TableData &data, int col)
{
    if (col < 0 || col >= data.columns.size())
        return false;
    int considered = 0;
    int numeric = 0;
    for (const QVector<QString> &row : data.rows) {
        const QString &v = row.value(col);
        if (v.trimmed().isEmpty())
            continue;
        ++considered;
        bool ok = false;
        toDouble(v, &ok);
        if (ok)
            ++numeric;
        if (considered >= 20)
            break;
    }
    return considered > 0 && numeric == considered;
}

bool DataInspector::columnLooksDateLike(const TableData &data, int col)
{
    if (col < 0 || col >= data.columns.size())
        return false;
    const QString name = foldTr(data.columns.value(col));
    static const QStringList nameHints = {
        QStringLiteral("date"), QStringLiteral("datetime"), QStringLiteral("time"),
        QStringLiteral("month"), QStringLiteral("ay"), QStringLiteral("tarih"),
        QStringLiteral("created"), QStringLiteral("year")};
    for (const QString &h : nameHints) {
        if (name.contains(h))
            return true;
    }
    int considered = 0;
    int iso = 0;
    for (const QVector<QString> &row : data.rows) {
        const QString &v = row.value(col);
        if (v.isEmpty())
            continue;
        ++considered;
        if (isoDateRe().match(v).hasMatch())
            ++iso;
        if (considered >= 10)
            break;
    }
    return considered > 0 && iso == considered;
}

bool DataInspector::columnLooksIdentifier(const TableData &data, int col)
{
    if (col < 0 || col >= data.columns.size())
        return false;
    const QString name = foldTr(data.columns.value(col));

    // Name-based surrogate/serial keys.
    static const QStringList exact = {QStringLiteral("id"), QStringLiteral("no"),
                                      QStringLiteral("numara"), QStringLiteral("key"),
                                      QStringLiteral("pk"), QStringLiteral("index"),
                                      QStringLiteral("seq"), QStringLiteral("sira"),
                                      QStringLiteral("rowindex"), QStringLiteral("rowid")};
    for (const QString &e : exact) {
        if (name == e)
            return true;
    }
    if (name.endsWith(QStringLiteral("_id")) || name.startsWith(QStringLiteral("id_"))
        || name.contains(QStringLiteral("numaras")))
        return true;

    // A strict, unique, consecutive integer sequence (1,2,3,...) is an ordinal
    // key, not a measure - even when the column has a neutral name.
    if (!columnLooksNumeric(data, col) || data.rows.size() < 2)
        return false;
    QVector<double> seq;
    seq.reserve(data.rows.size());
    for (const QVector<QString> &row : data.rows)
        seq << toDouble(row.value(col));
    std::sort(seq.begin(), seq.end());
    for (int i = 1; i < seq.size(); ++i) {
        if (seq[i] == seq[i - 1])
            return false; // duplicates -> not a key
    }
    const double start = seq.first();
    for (int i = 0; i < seq.size(); ++i) {
        if (std::fabs(seq[i] - std::round(seq[i])) > 1e-9)
            return false; // non-integer -> a real measure
        if (seq[i] != start + i)
            return false; // not consecutive -> could be a genuine metric
    }
    return true;
}

// Builds the multi-section analyst report (KPIs / trend / distribution /
// outliers / breakdown / recommendations) as HTML in Turkish + English.
static void buildAnalystReport(const TableData &data, const Insight &ins,
                               QString *trOut, QString *enOut)
{
    const int n = data.rows.size();
    if (n == 0) {
        trOut->clear();
        enOut->clear();
        return;
    }

    // ---- single row / aggregate: metric badges, no fake "#1" breakdown ----
    if (n == 1) {
        auto fmtVal = [](const QString &raw) -> QString {
            bool ok = false;
            const double v = toDouble(raw, &ok);
            return ok ? fmt(v) : raw;
        };
        auto metricBadge = [&fmtVal](const QString &k, const QString &v) {
            return QStringLiteral(
                "<div style='display:inline-block;margin:5px;padding:10px 16px;"
                "border:1px solid #dbeafe;border-radius:10px;background:#f5f8ff'>"
                "<div style='font-size:11px;color:#475467'>%1</div>"
                "<div style='font-size:22px;font-weight:800;color:#101828'>%2</div></div>")
                .arg(esc(k), esc(fmtVal(v)));
        };
        QString badges;
        for (int c = 0; c < data.columns.size(); ++c)
            badges += metricBadge(data.columns.at(c), data.rows.at(0).value(c));
        auto wrap = [&badges](const QString &title) {
            return QStringLiteral(
                "<div style='font-size:16px;font-weight:700;color:#101828'>%1</div>"
                "<div style='margin:10px 0'><div style='font-weight:700;color:#1d4ed8;"
                "border-bottom:1px solid #dbeafe;padding-bottom:2px;margin-bottom:4px'>Tekil Metrik "
                "(Scalar/Aggregate)</div>%2</div>"
                "<div style='color:#475467;font-size:13px'>Bu sorgu tek bir satir/aggregate deger "
                "dondurdu; grafik yerine KPI metrigi olarak sunuldu.</div>")
                .arg(title, badges);
        };
        auto wrapEn = [&badges](const QString &title) {
            return QStringLiteral(
                "<div style='font-size:16px;font-weight:700;color:#101828'>%1</div>"
                "<div style='margin:10px 0'><div style='font-weight:700;color:#1d4ed8;"
                "border-bottom:1px solid #dbeafe;padding-bottom:2px;margin-bottom:4px'>Single Metric "
                "(Scalar/Aggregate)</div>%2</div>"
                "<div style='color:#475467;font-size:13px'>This query returned a single "
                "row/aggregate value; presented as a KPI metric instead of a chart.</div>")
                .arg(title, badges);
        };
        *trOut = wrap(QStringLiteral("Yonetici Ozeti"));
        *enOut = wrapEn(QStringLiteral("Executive Summary"));
        return;
    }
    const int xcol = ins.xColumn;
    const int ycol = ins.yColumn;
    const bool countMode = (ycol < 0);

    auto labelAt = [&](int r) -> QString {
        if (xcol >= 0) {
            const QString l = data.rows.at(r).value(xcol);
            return l.isEmpty() ? QStringLiteral("#%1").arg(r + 1) : l;
        }
        return QStringLiteral("#%1").arg(r + 1);
    };

    QVector<double> values;
    values.reserve(n);
    for (int r = 0; r < n; ++r)
        values << (countMode ? 1.0 : toDouble(data.rows.at(r).value(ycol)));

    double total = 0.0, mn = 0.0, mx = 0.0;
    for (int i = 0; i < n; ++i) {
        total += values[i];
        if (i == 0 || values[i] < mn) mn = values[i];
        if (i == 0 || values[i] > mx) mx = values[i];
    }
    const double mean = total / n;
    QVector<double> sorted = values;
    std::sort(sorted.begin(), sorted.end());
    const double median = quantile(sorted, 0.5);
    double var = 0.0;
    for (double v : values)
        var += (v - mean) * (v - mean);
    const double stdev = std::sqrt(var / n);

    QSet<QString> distinctSet;
    QMap<QString, double> agg;
    QStringList order;
    for (int r = 0; r < n; ++r) {
        const QString l = labelAt(r);
        distinctSet.insert(l);
        if (!agg.contains(l))
            order << l;
        agg[l] += values[r];
    }
    QVector<Bucket> buckets;
    for (const QString &l : order) {
        Bucket b;
        b.label = l;
        b.value = agg.value(l);
        b.share = total > 1e-9 ? b.value * 100.0 / total : 0.0;
        buckets << b;
    }
    std::sort(buckets.begin(), buckets.end(),
              [](const Bucket &a, const Bucket &b) { return a.value > b.value; });
    const int distinct = distinctSet.size();
    const double topShare = buckets.isEmpty() ? 0.0 : buckets.first().share;

    bool hasTrend = false;
    double trendPct = 0.0;
    if (!countMode && n >= 2 && std::fabs(values.first()) > 1e-9) {
        hasTrend = true;
        trendPct = (values.last() - values.first()) * 100.0 / values.first();
    }

    QVector<QPair<QString, double>> outliers;
    if (!countMode && stdev > 1e-9) {
        for (int r = 0; r < n; ++r)
            if (std::fabs(values[r] - mean) > 2.0 * stdev)
                outliers << qMakePair(labelAt(r), values[r]);
    }

    auto badge = [](const QString &k, const QString &v) {
        return QStringLiteral(
            "<td style='padding:4px 6px;text-align:center'>"
            "<div style='font-size:10px;color:#667085'>%1</div>"
            "<div style='font-size:15px;font-weight:700;color:#101828'>%2</div></td>")
            .arg(esc(k), esc(v));
    };
    auto kpiTable = [&](const QStringList &labels, const QStringList &vals) {
        QString cells;
        for (int i = 0; i < labels.size(); ++i) {
            if (i % 4 == 0)
                cells += QStringLiteral("<tr>");
            cells += badge(labels.at(i), vals.at(i));
            if (i % 4 == 3 || i == labels.size() - 1)
                cells += QStringLiteral("</tr>");
        }
        return QStringLiteral("<table cellspacing=0 cellpadding=0 style='width:100%'>")
               + cells + QStringLiteral("</table>");
    };
    auto section = [](const QString &title, const QString &body) {
        return QStringLiteral(
            "<div style='margin:10px 0'><div style='font-weight:700;color:#1d4ed8;"
            "border-bottom:1px solid #dbeafe;padding-bottom:2px;margin-bottom:4px'>%1</div>%2</div>")
            .arg(title, body);
    };
    auto distTable = [&]() {
        QString body = QStringLiteral(
            "<table cellspacing=0 cellpadding=0 style='width:100%;font-size:13px'>");
        const int show = std::min(3, static_cast<int>(buckets.size()));
        for (int i = 0; i < show; ++i) {
            const Bucket &b = buckets.at(i);
            body += QStringLiteral(
                "<tr><td style='padding:2px 4px'>%1) %2</td>"
                "<td style='padding:2px 4px;text-align:right;font-weight:600'>%3</td>"
                "<td style='padding:2px 4px;text-align:right;color:#475467'>%4%</td></tr>")
                .arg(i + 1).arg(esc(b.label), fmt(b.value))
                .arg(QString::number(b.share, 'f', 1));
        }
        return body + QStringLiteral("</table>");
    };

    const QStringList kpiVals = {QString::number(n), QString::number(distinct),
                                 fmt(total), fmt(mean), fmt(median), fmt(mn),
                                 fmt(mx), fmt(stdev)};

    // ---- Turkish ----
    {
        QString html = QStringLiteral(
            "<div style='font-size:16px;font-weight:700;color:#101828'>Yonetici Ozeti</div>");
        html += section(QStringLiteral("Temel Metrikler (KPI)"),
                       kpiTable({QStringLiteral("Satir"), QStringLiteral("Benzersiz"),
                                 QStringLiteral("Toplam"), QStringLiteral("Ortalama"),
                                 QStringLiteral("Medyan"), QStringLiteral("Min"),
                                 QStringLiteral("Maks"), QStringLiteral("Std.Sapma")}, kpiVals));
        if (hasTrend) {
            const bool up = trendPct >= 0;
            html += section(QStringLiteral("Trend"),
                            QStringLiteral("<div>Ilk donemden son doneme <b>%1%2%</b> (%3) yonlu degisim. "
                                           "Baslangic %4 -> Bitis %5.</div>")
                                .arg(up ? QStringLiteral("+") : QString())
                                .arg(QString::number(std::fabs(trendPct), 'f', 1))
                                .arg(up ? QStringLiteral("yukselis") : QStringLiteral("dusus"),
                                     fmt(values.first()), fmt(values.last())));
        }
        QString dist = distTable();
        const double evenShare = buckets.isEmpty() ? 0.0 : 100.0 / buckets.size();
        const double secondShare = buckets.size() > 1 ? buckets.at(1).share : 0.0;
        const bool nearEven = buckets.size() > 1 && (topShare - secondShare) <= 5.0;
        if (nearEven || topShare <= evenShare + 2.0)
            dist += QStringLiteral("<div style='margin-top:4px;color:#475467'>Dagilim dengeli: "
                                   "kategori paylari birbirine yakin (en buyuk %1%).</div>")
                        .arg(QString::number(topShare, 'f', 1));
        else if (topShare >= 50.0)
            dist += QStringLiteral("<div style='margin-top:4px;color:#b54708'><b>Yogunlasma yuksek:</b> "
                                   "toplamin %1%'i tek kategori kaynakli (%2).</div>")
                        .arg(QString::number(topShare, 'f', 1), esc(buckets.first().label));
        else if (topShare >= 30.0)
            dist += QStringLiteral("<div style='margin-top:4px;color:#475467'>Lider kategori payi %1%.</div>")
                        .arg(QString::number(topShare, 'f', 1));
        html += section(QStringLiteral("Dagilim (en yuksek katki)"), dist);

        QString out;
        if (outliers.isEmpty())
            out = QStringLiteral("<div style='color:#475467'>Ortalama +/- 2s bandinda aykiri deger bulunmadi.</div>");
        else {
            out = QStringLiteral("<div>Ortalama +/- 2s disinda <b>%1</b> gozlem:</div><ul>").arg(outliers.size());
            for (const auto &o : outliers)
                out += QStringLiteral("<li>%1 -> %2</li>").arg(esc(o.first), fmt(o.second));
            out += QStringLiteral("</ul>");
        }
        html += section(QStringLiteral("Aykiri Degerler"), out);

        QStringList sugg;
        if (!buckets.isEmpty())
            sugg << QStringLiteral("En yuksek katki '%1' = %2 (%3%); bu kaleme odaklan.")
                        .arg(esc(buckets.first().label), fmt(buckets.first().value))
                        .arg(QString::number(topShare, 'f', 1));
        if (hasTrend)
            sugg << (trendPct >= 0
                          ? QStringLiteral("Serit yukselis trendinde (+%1%); momentumu koru.")
                                .arg(QString::number(trendPct, 'f', 1))
                          : QStringLiteral("Serit dusus trendinde (-%1%); nedenleri arastir.")
                                .arg(QString::number(std::fabs(trendPct), 'f', 1)));
        if (!countMode && mean > 1e-9 && stdev / mean > 0.5)
            sugg << QStringLiteral("Degiskenlik yuksek (Std/Ortalama = %1); tahminlerde temkinli ol.")
                        .arg(QString::number(stdev / mean, 'f', 2));
        if (sugg.isEmpty())
            sugg << QStringLiteral("Veri dengeli gorunuyor; orneklemi buyutup tekrar analiz et.");
        QString sug = QStringLiteral("<ul>");
        for (const QString &s : sugg)
            sug += QStringLiteral("<li>%1</li>").arg(esc(s));
        sug += QStringLiteral("</ul>");
        html += section(QStringLiteral("Oneriler"), sug);
        *trOut = html;
    }

    // ---- English ----
    {
        QString html = QStringLiteral(
            "<div style='font-size:16px;font-weight:700;color:#101828'>Executive Summary</div>");
        html += section(QStringLiteral("Key Metrics (KPI)"),
                       kpiTable({QStringLiteral("Rows"), QStringLiteral("Distinct"),
                                 QStringLiteral("Total"), QStringLiteral("Mean"),
                                 QStringLiteral("Median"), QStringLiteral("Min"),
                                 QStringLiteral("Max"), QStringLiteral("Std")}, kpiVals));
        if (hasTrend) {
            const bool up = trendPct >= 0;
            html += section(QStringLiteral("Trend"),
                            QStringLiteral("<div>First-to-last change <b>%1%2%</b> (%3). "
                                           "Start %4 -> End %5.</div>")
                                .arg(up ? QStringLiteral("+") : QString())
                                .arg(QString::number(std::fabs(trendPct), 'f', 1))
                                .arg(up ? QStringLiteral("rising") : QStringLiteral("falling"),
                                     fmt(values.first()), fmt(values.last())));
        }
        QString dist = distTable();
        const double evenShare = buckets.isEmpty() ? 0.0 : 100.0 / buckets.size();
        const double secondShare = buckets.size() > 1 ? buckets.at(1).share : 0.0;
        const bool nearEven = buckets.size() > 1 && (topShare - secondShare) <= 5.0;
        if (nearEven || topShare <= evenShare + 2.0)
            dist += QStringLiteral("<div style='margin-top:4px;color:#475467'>Balanced distribution: "
                                   "category shares are close (largest %1%).</div>")
                        .arg(QString::number(topShare, 'f', 1));
        else if (topShare >= 50.0)
            dist += QStringLiteral("<div style='margin-top:4px;color:#b54708'><b>High concentration:</b> "
                                   "%1% of the total comes from one category (%2).</div>")
                        .arg(QString::number(topShare, 'f', 1), esc(buckets.first().label));
        else if (topShare >= 30.0)
            dist += QStringLiteral("<div style='margin-top:4px;color:#475467'>Leading category share %1%.</div>")
                        .arg(QString::number(topShare, 'f', 1));
        html += section(QStringLiteral("Distribution (top contributors)"), dist);

        QString out;
        if (outliers.isEmpty())
            out = QStringLiteral("<div style='color:#475467'>No observations beyond mean +/- 2 sigma.</div>");
        else {
            out = QStringLiteral("<div><b>%1</b> observation(s) beyond mean +/- 2 sigma:</div><ul>").arg(outliers.size());
            for (const auto &o : outliers)
                out += QStringLiteral("<li>%1 -> %2</li>").arg(esc(o.first), fmt(o.second));
            out += QStringLiteral("</ul>");
        }
        html += section(QStringLiteral("Outliers"), out);

        QStringList sugg;
        if (!buckets.isEmpty())
            sugg << QStringLiteral("Top contributor '%1' = %2 (%3%); focus on this driver.")
                        .arg(esc(buckets.first().label), fmt(buckets.first().value))
                        .arg(QString::number(topShare, 'f', 1));
        if (hasTrend)
            sugg << (trendPct >= 0
                          ? QStringLiteral("Series trending up (+%1%); sustain the momentum.")
                                .arg(QString::number(trendPct, 'f', 1))
                          : QStringLiteral("Series trending down (-%1%); investigate and remediate.")
                                .arg(QString::number(std::fabs(trendPct), 'f', 1)));
        if (!countMode && mean > 1e-9 && stdev / mean > 0.5)
            sugg << QStringLiteral("High variability (Std/Mean = %1); be cautious with forecasts.")
                        .arg(QString::number(stdev / mean, 'f', 2));
        if (sugg.isEmpty())
            sugg << QStringLiteral("Data looks balanced; enlarge the sample and re-analyze.");
        QString sug = QStringLiteral("<ul>");
        for (const QString &s : sugg)
            sug += QStringLiteral("<li>%1</li>").arg(esc(s));
        sug += QStringLiteral("</ul>");
        html += section(QStringLiteral("Recommendations"), sug);
        *enOut = html;
    }
}

Insight DataInspector::analyze(const TableData &data, ChartType forcedType,
                               int forcedX, int forcedY)
{
    Insight out;
    if (data.isEmpty() || data.columns.size() < 1)
        return out;

    const int cols = data.columns.size();

    // ---- choose Y (numeric measure) -------------------------------------
    int y = forcedY;
    if (y < 0 || y >= cols) {
        y = -1;
        // Prefer named measures (toplam/sum/total/ciro/tutar) then any numeric.
        int bestBonus = -1;
        for (int c = 0; c < cols; ++c) {
            if (!columnLooksNumeric(data, c))
                continue;
            if (columnLooksIdentifier(data, c))
                continue; // never measure by a surrogate/serial key (id, *_id, ...)
            const QString name = foldTr(data.columns.at(c));
            int bonus = 1;
            if (name.contains(QStringLiteral("toplam")) || name.contains(QStringLiteral("sum"))
                || name.contains(QStringLiteral("total")) || name.contains(QStringLiteral("tutar"))
                || name.contains(QStringLiteral("ciro")) || name.contains(QStringLiteral("amount")))
                bonus = 3;
            if (bonus > bestBonus) {
                bestBonus = bonus;
                y = c;
            }
        }
    }

    // ---- choose X (label / date axis) ------------------------------------
    int x = forcedX;
    if (x < 0 || x >= cols || x == y) {
        x = -1;
        for (int c = 0; c < cols; ++c) {
            if (c == y || columnLooksNumeric(data, c))
                continue;
            x = c;
            break;
        }
        if (x < 0) {
            for (int c = 0; c < cols; ++c) {
                if (c == y)
                    continue;
                x = c;
                break;
            }
        }
    }

    if (y < 0) {
        // No numeric measure: still chart row counts per first text column.
        if (x < 0)
            return out;
        y = -1; // signals count-mode; keep summary simple below
    }

    // ---- single-row / aggregate -> KPI metric, not a chart ---------------
    if (data.rows.size() == 1) {
        auto fmtVal = [](const QString &raw) -> QString {
            bool ok = false;
            const double v = toDouble(raw, &ok);
            return ok ? fmt(v) : raw;
        };
        out.isScalar = true;
        out.chartType = ChartType::None; // never try to plot a lone record
        out.xColumn = x;
        out.yColumn = y;
        for (int c = 0; c < cols; ++c)
            out.kpiItems.append(qMakePair(data.columns.at(c), data.rows.at(0).value(c)));
        const int head = (y >= 0) ? y : 0; // prefer the numeric measure
        out.kpiLabel = data.columns.at(head);
        out.kpiValue = data.rows.at(0).value(head);
        const QString headVal = fmtVal(out.kpiValue);
        if (cols == 1) {
            out.summaryTr = QStringLiteral("Tekil deger: %1 = %2.").arg(out.kpiLabel, headVal);
            out.summaryEn = QStringLiteral("Single value: %1 = %2.").arg(out.kpiLabel, headVal);
        } else {
            QStringList pairs;
            for (const auto &kv : out.kpiItems)
                pairs << QStringLiteral("%1=%2").arg(kv.first, fmtVal(kv.second));
            out.summaryTr = QStringLiteral("Tek satirlik aggregate sonuc (%1 metrik): %2.")
                                .arg(cols).arg(pairs.join(QStringLiteral(", ")));
            out.summaryEn = QStringLiteral("Single-row aggregate result (%1 metrics): %2.")
                                .arg(cols).arg(pairs.join(QStringLiteral(", ")));
        }
        buildAnalystReport(data, out, &out.reportHtmlTr, &out.reportHtmlEn);
        return out;
    }

    // ---- chart type -------------------------------------------------------
    const bool xIsDate = x >= 0 && columnLooksDateLike(data, x);
    const int rowCount = data.rows.size();
    ChartType type = forcedType;
    if (type == ChartType::None) {
        if (y >= 0 && (xIsDate || (rowCount >= 6 && x >= 0 && columnLooksNumeric(data, x))))
            type = ChartType::Line; // trend over time / ordered axis
        else if (y >= 0 && rowCount <= 8 && x >= 0)
            type = ChartType::Pie;  // small category breakdown
        else
            type = ChartType::Bar;
    }

    out.chartType = type;
    out.xColumn = x;
    out.yColumn = y;

    // ---- statistics for the executive summaries ---------------------------
    double total = 0.0;
    double maxV = 0.0;
    double minV = 0.0;
    int maxRow = -1;
    int minRow = -1;
    if (y >= 0) {
        bool first = true;
        for (int r = 0; r < rowCount; ++r) {
            const double v = toDouble(data.rows.at(r).value(y));
            total += v;
            if (first || v > maxV) { maxV = v; maxRow = r; first = false; }
            if (minRow < 0 || v < minV) { minV = v; minRow = r; }
        }
    }

    const QString xLabel = x >= 0 ? data.columns.at(x) : QString();
    const QString yLabel = y >= 0 ? data.columns.at(y) : QStringLiteral("say");
    const QString peakLabel = (x >= 0 && maxRow >= 0) ? data.rows.at(maxRow).value(x) : QString();

    QString growthTr;
    QString growthEn;
    if (type == ChartType::Line && rowCount >= 2 && y >= 0) {
        const double firstV = toDouble(data.rows.first().value(y));
        const double lastV = toDouble(data.rows.last().value(y));
        if (std::fabs(firstV) > 1e-9) {
            const double pct = (lastV - firstV) * 100.0 / firstV;
            growthTr = QStringLiteral(" Ilk dönemden son döneme %L1%2 degisim.")
                           .arg(pct >= 0 ? QStringLiteral("+") : QString())
                           .arg(QString::number(pct, 'f', 1));
            growthEn = QStringLiteral(" Change from first to last period: %1%2%.")
                           .arg(pct >= 0 ? QStringLiteral("+") : QString())
                           .arg(QString::number(pct, 'f', 1));
        }
    }

    if (y >= 0) {
        const QString peakPartTr = peakLabel.isEmpty()
            ? QStringLiteral(", en yuksek %1").arg(fmt(maxV))
            : QStringLiteral(", en yuksek %1 (%2)").arg(fmt(maxV), peakLabel);
        const QString peakPartEn = peakLabel.isEmpty()
            ? QStringLiteral("; peak %1").arg(fmt(maxV))
            : QStringLiteral("; peak %1 (%2)").arg(fmt(maxV), peakLabel);
        out.summaryTr = QStringLiteral("Genel bakis: %1 satir, %2 toplam %3.")
                            .arg(rowCount).arg(yLabel, fmt(total)) + peakPartTr + growthTr;
        out.summaryEn = QStringLiteral("Overview: %1 rows, total %2 = %3.")
                            .arg(rowCount).arg(yLabel, fmt(total)) + peakPartEn + growthEn;
    } else {
        out.summaryTr = QStringLiteral("Genel bakis: %1 satir; sayisal olcek kolonu yok, "
                                       "satir sayisina gore grafik cizildi.").arg(rowCount);
        out.summaryEn = QStringLiteral("Overview: %1 rows; no numeric measure found, "
                                       "plotted row counts instead.").arg(rowCount);
    }

    // Rich multi-section analyst report (works in count-mode too).
    buildAnalystReport(data, out, &out.reportHtmlTr, &out.reportHtmlEn);
    return out;
}

} // namespace ai
