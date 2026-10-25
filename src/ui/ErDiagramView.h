#pragma once

#include <QGraphicsView>
#include <QHash>
#include <QPointer>
#include <QString>
#include <QVector>

#include "core/DatabaseManager.h"

class QLabel;
class QContextMenuEvent;
class QPushButton;
class QTouchEvent;
class QWidget;

namespace ui {

class TableNodeItem;
class ErEdgeItem;

/// Interactive ER diagram: every table is a draggable, premium-styled card
/// (TableNodeItem) and every FOREIGN KEY becomes a live edge that keeps its
/// endpoints welded to the card borders while dragging. Positions persist per
/// database/table in QSettings ("QuickDb/er/<dbId>/<table>" = "x,y"); a
/// floating zoom strip ([+] [−] [reset %] [Fit]) overlays the viewport and
/// Ctrl+wheel zooms with an anchor under the mouse. Rebuilt from the live
/// schema via rebuild().
class ErDiagramView : public QGraphicsView {
    Q_OBJECT

public:
    explicit ErDiagramView(core::DatabaseManager *db, QWidget *parent = nullptr);

    /// Reparses the schema and redraws the whole scene (saved positions are
    /// re-applied; unsaved nodes fall onto the automatic grid).
    void rebuild();

    /// Scales/pans so the whole diagram fits the viewport.
    void fitToContents();

    /// Drops every saved position and returns to the automatic grid.
    void resetLayout();

    // Zoom API shared by the floating strip and the headless tests.
    qreal currentZoom() const { return m_zoom; }
    void zoomIn() { applyZoom(1.25); }
    void zoomOut() { applyZoom(1.0 / 1.25); }
    void resetZoom() { applyZoom(1.0 / m_zoom); }

    // Test/automation helpers: node positions round-trip through QSettings.
    bool hasTable(const QString &name) const { return m_nodes.contains(name); }
    void setTablePos(const QString &name, QPointF pos);
    QPointF tablePos(const QString &name) const;

    /// Re-applies theme-sensitive styling (dot-grid background, zoom strip).
    void refresh();

    /// Called by a dragging node; writes the position to QSettings.
    void persistPosition(const QString &table, QPointF scenePos);

    // Introspection for tests / status messages.
    int tableCount() const { return m_tableCount; }
    int relationCount() const { return m_relationCount; }

protected:
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    bool viewportEvent(QEvent *event) override;

private:
    void applyZoom(qreal factor);
    void setZoomValue(qreal z);
    void updateZoomLabel();
    void positionControls();
    void styleControls();
    void applyGridBackground();
    QString dbIdentifier() const;
    QString settingsPrefix() const;

    core::DatabaseManager *m_db;
    int m_tableCount = 0;
    int m_relationCount = 0;
    qreal m_zoom = 1.0;

    QHash<QString, QPointer<TableNodeItem>> m_nodes;
    QVector<ErEdgeItem *> m_edges;

    // Floating zoom strip (child of the viewport, pinned top-left).
    QWidget *m_controls = nullptr;
    QPushButton *m_btnIn = nullptr;
    QPushButton *m_btnOut = nullptr;
    QPushButton *m_btnReset = nullptr;
    QPushButton *m_btnFit = nullptr;
    QLabel *m_zoomLabel = nullptr;

    // Pinch-zoom state for trackpad/touch gestures.
    qreal m_zoomAtTouchStart = 1.0;
    qreal m_touchDistance = 0.0;
};

} // namespace ui
