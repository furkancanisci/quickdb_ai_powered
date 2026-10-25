#pragma once

#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

#include "core/DatabaseManager.h"

namespace ai {

/// Snapshot of a result set (values stringified) — the query is forward-only,
/// so everything the analyzer and the chart need must be materialized once.
struct TableData {
    QStringList columns;
    QVector<QVector<QString>> rows;

    bool isEmpty() const { return rows.isEmpty(); }

    /// Materializes every row of a successful result set.
    static TableData fromQueryResult(const core::QueryResult &result);
};

/// Heuristic chart-type decision.
enum class ChartType { Line, Bar, Pie, None };

inline QString chartTypeName(ChartType t)
{
    switch (t) {
    case ChartType::Line: return QStringLiteral("LineChart");
    case ChartType::Bar:  return QStringLiteral("BarChart");
    case ChartType::Pie:  return QStringLiteral("PieChart");
    case ChartType::None: break;
    }
    return QStringLiteral("None");
}

/// Result of analyzing a result table: axis choice, chart type and executive
/// summaries in Turkish and English, generated from plain statistics
/// (total / max / min / growth) — deterministic, no LLM involved.
struct Insight {
    ChartType chartType = ChartType::None;
    int xColumn = -1;
    int yColumn = -1;
    QString summaryTr;
    QString summaryEn;
    /// Multi-section business-analyst report (KPIs, trend, distribution,
    /// outliers, breakdown + recommendations) as self-contained HTML.
    QString reportHtmlTr;
    QString reportHtmlEn;

    /// Single-row / aggregate result: better shown as a KPI metric than a
    /// chart. When true, `chartType` is forced to `None` and the `kpi*`
    /// fields describe the lone record.
    bool isScalar = false;
    QString kpiLabel;                              // headline metric name
    QString kpiValue;                              // headline metric value
    QVector<QPair<QString, QString>> kpiItems;     // (column, value) for every field

    bool plottable() const { return !isScalar && chartType != ChartType::None && xColumn >= 0 && yColumn >= 0; }
};

class DataInspector {
public:
    /// Picks axes + chart type and writes TR/EN summaries.
    /// forcedX/forcedY (>=0) override the automatic column choice.
    /// forcedType != None overrides the automatic chart type.
    static Insight analyze(const TableData &data,
                           ChartType forcedType = ChartType::None,
                           int forcedX = -1, int forcedY = -1);

    /// Column heuristics shared with tests.
    static bool columnLooksNumeric(const TableData &data, int col);
    static bool columnLooksDateLike(const TableData &data, int col);
    /// True for surrogate/serial keys (id, *_id, no, sira, or a strict 1..n
    /// integer sequence). Such columns must never be chosen as a Y-axis metric.
    static bool columnLooksIdentifier(const TableData &data, int col);
};

} // namespace ai
