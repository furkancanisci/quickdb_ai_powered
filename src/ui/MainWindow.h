#pragma once

#include <QMainWindow>

#include <QStringList>
#include <QVector>

#include "core/DatabaseManager.h"
#include "core/io/Exporters.h"

class QAction;
class QCheckBox;
class QLabel;
class QPushButton;
class QSqlTableModel;
class QTabWidget;
class QTreeView;
class QModelIndex;
class QTableView;

namespace ai { struct TableData; }
namespace core { class DashboardServer; }

namespace ui {

class AiAssistantPanel;
class ErDiagramView;
class QueryEditor;
class ResultTab;
class SchemaTreeModel;
class SelectQueryModel;

/// Main application window: sidebar tree on the left, result grid on top
/// and SQL editor below it on the right. All database work is delegated
/// to core::DatabaseManager.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /// The QTableView of the currently active result tab (empty tab view if
    /// none). Used by the headless UI test to inspect the live grid.
    QTableView *currentResultView() const;

private slots:
    // Connection lifecycle
    void newDatabase();
    void openDatabase();
    void closeDatabase();
    void openConnectionDialog();   // driver-agnostic Connection Manager

    // Query execution
    void executeSql();
    void refreshSchema();
    void explainCurrentQuery();     // EXPLAIN QUERY PLAN -> new result tab
    void formatCurrentSql();        // basic keyword-newline formatter

    // DDL UI hooks
    void createTableViaDialog();
    void addColumnViaDialog();
    void dropSelectedTable();

    // Schema object management (Phase F)
    void createViewViaDialog();
    void createIndexViaDialogForTable(const QString &table);
    void showObjectDdl(const QString &objectName);
    void dropTreeObject();
    void openViewTab(const QString &view);

    // Import / Export
    void exportActiveResult();       // serialize the active result tab to a file
    void exportSelectedTable();      // serialize a tree-selected table via SELECT *
    void importFile();               // guided CSV/JSON import into a table

    // Sidebar interactions
    void onTreeDoubleClicked(const QModelIndex &index);
    void onTreeContextMenu(const QPoint &pos);

    // Grid CRUD (table-browse mode)
    void addRowToTable();
    void deleteCurrentRow();
    void saveTableChanges();
    void revertTableChanges();
    void onResultContextMenu(const QPoint &pos);

    /// Opens/creates the given file and wires up UI state. Also used by the
    /// automated UI integration test (invokeMethod) to avoid modal dialogs.
    void connectToPath(const QString &filePath, bool create);

    // AI assistant
    void onAiInsertSql(const QString &sql);      // load chat SQL into editor
    void onAiPresentResult(const QString &sql, const ai::TableData &data,
                           const QString &source, const QString &message);
    void updateAiBadge(bool available, const QStringList &models);

    // Web dashboard (localhost)
    void openDashboard();        // start server + open in browser
    void onPinToggled(bool on);  // pin/unpin the active result tab

private:
    void setupUi();
    void setupActions();
    void showStatusMessage(const QString &message, bool isError = false);
    void updateConnectionStatus();
    Q_INVOKABLE void openTableTab(const QString &table); // browse tab (reuse if open)
    void openQueryTab(core::QueryResult &result, const QString &sql); // new read-only tab
    void openSnapshotTab(const ai::TableData &data, const QString &sql); // read-only from cached rows
    void closeAllTabs();                            // drop every result tab
    ResultTab *activeResultTab() const;
    void updateActiveTabChrome();                   // sync CRUD toolbar to active tab
    bool collectTableData(ResultTab *tab, QStringList *columns,
                          QVector<QVector<QString>> *rows) const; // snapshot of a tab
    void syncPinCheckbox();                         // reflect active tab's pinned state
    Q_INVOKABLE void onTabCloseRequested(int index);
    void attachResultTab(ResultTab *tab);           // per-view context menu wiring
    int addResultTab(ResultTab *tab, const QString &title); // add + recycle placeholder
    void setBrowseControlsEnabled(bool enabled);
    QString selectedTableName() const;              // from tree selection
    bool collectExportData(ResultTab *tab, core::io::ExportInput *out) const;

    core::DatabaseManager m_db;

    // Sidebar
    QTreeView *m_tree = nullptr;
    SchemaTreeModel *m_treeModel = nullptr;

    // Editor
    QueryEditor *m_editor = nullptr;
    QPushButton *m_executeButton = nullptr;

    // Results: multiple tabs, each a ResultTab (view + model + mode).
    QTabWidget *m_resultTabs = nullptr;
    int m_queryTabSeq = 0;

    // Shared CRUD toolbar (drives the active ResultTab).
    QPushButton *m_addRowButton = nullptr;
    QPushButton *m_deleteRowButton = nullptr;
    QPushButton *m_saveButton = nullptr;
    QPushButton *m_revertButton = nullptr;
    QLabel *m_browseLabel = nullptr;

    // Web dashboard controls (live in the same CRUD row).
    QCheckBox *m_pinCheck = nullptr;
    QPushButton *m_dashboardButton = nullptr;
    core::DashboardServer *m_dashboard = nullptr;

    // ER diagram tab
    ErDiagramView *m_erView = nullptr;

    // Center tabbed area (SQL & Data + ER) and the right-hand AI dock.
    QTabWidget *m_centerTabs = nullptr;
    AiAssistantPanel *m_aiPanel = nullptr;
    QLabel *m_aiBadge = nullptr;

    // Status bar
    QLabel *m_connectionLabel = nullptr;

    // Actions
    QAction *m_actNew = nullptr;
    QAction *m_actOpen = nullptr;
    QAction *m_actClose = nullptr;
    QAction *m_actExecute = nullptr;
    QAction *m_actRefreshSchema = nullptr;
    QAction *m_actExplain = nullptr;
    QAction *m_actFormat = nullptr;
    QAction *m_actCreateTable = nullptr;
    QAction *m_actAddColumn = nullptr;
    QAction *m_actDropTable = nullptr;
    QAction *m_actExport = nullptr;
    QAction *m_actExportTable = nullptr;
    QAction *m_actImport = nullptr;
    QAction *m_actQuit = nullptr;
    QAction *m_actAutoFormat = nullptr; // View ▸ Auto-Format SQL (persisted)
};

} // namespace ui
