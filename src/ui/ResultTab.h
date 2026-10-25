#pragma once

#include <QWidget>

#include "core/DatabaseManager.h"

class QTableView;
class QSqlTableModel;
class QAbstractItemModel;
class QStandardItemModel;

namespace ui {

class SelectQueryModel;

/// One result surface: a QTableView backed either by a read-only
/// SelectQueryModel (query mode) or an editable QSqlTableModel (browse mode).
/// Owns its model + connection handle and exposes the CRUD primitives so the
/// MainWindow can drive the active tab through a small, stable API.
class ResultTab : public QWidget {
    Q_OBJECT

public:
    enum class Mode { Query, Browse, Snapshot };

    explicit ResultTab(QWidget *parent = nullptr);

    // --- Setup ----------------------------------------------------------
    /// Populates a read-only query tab from an executed result set (consumes
    /// the result's live query handle).
    void runQuery(core::QueryResult &result, QSqlDatabase db, const QString &sql);
    /// Loads an editable table-browse tab (staged until save).
    void browseTable(const QString &table, QSqlDatabase db);
    /// Shows an already-materialized result (columns + string rows) read-only,
    /// without touching the database — used to present the AI's live result.
    void showSnapshot(const QStringList &columns, const QVector<QVector<QString>> &rows,
                      const QString &sql);

    // --- Accessors ------------------------------------------------------
    QTableView *view() const { return m_view; }
    Mode mode() const { return m_mode; }
    QString table() const { return m_table; }
    QString sql() const { return m_sql; }
    QAbstractItemModel *model() const;
    bool isBrowseEditable() const { return m_mode == Mode::Browse && m_tableModel; }

    // --- CRUD (delegated from the MainWindow's global toolbar) ---------
    bool addRow();
    bool deleteCurrentRow();
    QString save();     // "" on success, otherwise the error text
    void revert();
    bool isDirty() const;

    void refreshView();
    /// Releases live query/model handles before the tab is destroyed or the
    /// database connection goes away.
    void releaseData();

private:
    void configureView();

    QTableView *m_view = nullptr;
    SelectQueryModel *m_queryModel = nullptr;
    QSqlTableModel *m_tableModel = nullptr;
    QStandardItemModel *m_snapshotModel = nullptr;
    QSqlDatabase m_db;
    Mode m_mode = Mode::Query;
    QString m_table;
    QString m_sql;
    bool m_dirty = false;
};

} // namespace ui
