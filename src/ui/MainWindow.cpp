#include "ui/MainWindow.h"

#include <QAction>
#include <QAbstractItemModel>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDebug>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlTableModel>
#include <QSettings>
#include <QStatusBar>
#include <QTableView>
#include <QTabWidget>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include "core/web/DashboardServer.h"
#include "ui/Dialogs/AddColumnDialog.h"
#include "ui/Dialogs/ConnectionDialog.h"
#include "ui/Dialogs/CreateIndexDialog.h"
#include "ui/Dialogs/CreateTableDialog.h"
#include "ui/Dialogs/CreateViewDialog.h"
#include "ui/Dialogs/ExportDialog.h"
#include "ui/Dialogs/ImportDialog.h"
#include "ai/OllamaClient.h"
#include "ui/AiAssistantPanel.h"
#include "ui/ErDiagramView.h"
#include "ui/QueryEditor.h"
#include "ui/ResultTab.h"
#include "ui/SchemaTreeModel.h"
#include "ui/SelectQueryModel.h"
#include "ui/Theme.h"

namespace ui {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("QuickDb Pro \u2014 SQL & AI Analytics"));
    resize(1200, 760);
    setMinimumSize(980, 620);

    // macOS: keep menus inside our window instead of the native menu bar.
    // The native bar can swallow actions when the bundle isn't the active
    // foreground app (e.g. launched from a terminal), making buttons look dead.
    menuBar()->setNativeMenuBar(false);

    setupUi();
    setupActions();
    updateConnectionStatus();
    refreshSchema();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    // --- Left: schema sidebar ------------------------------------------
    m_treeModel = new SchemaTreeModel(&m_db, this);
    m_tree = new QTreeView;
    m_tree->setModel(m_treeModel);
    m_tree->setHeaderHidden(true);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setObjectName(QStringLiteral("schemaTree"));

    auto *sidebar = new QGroupBox(QStringLiteral("Databases"));
    auto *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(6, 6, 6, 6);
    sidebarLayout->addWidget(m_tree);

    // --- Right: results on top, SQL editor below ------------------------
    m_editor = new QueryEditor;
    m_editor->setObjectName(QStringLiteral("queryEditor"));
    m_executeButton = new QPushButton(QStringLiteral("Execute  (⌘/Ctrl+Enter)"));
    m_executeButton->setObjectName(QStringLiteral("executeButton"));
    m_executeButton->setMinimumHeight(34);

    auto *editorBox = new QGroupBox(QStringLiteral("SQL Editor"));
    auto *editorLayout = new QVBoxLayout(editorBox);
    editorLayout->setContentsMargins(6, 6, 6, 6);
    editorLayout->addWidget(m_editor);
    editorLayout->addWidget(m_executeButton);

    m_resultTabs = new QTabWidget;
    m_resultTabs->setObjectName(QStringLiteral("resultTabs"));
    m_resultTabs->setTabsClosable(true);
    m_resultTabs->setMovable(true);
    m_resultTabs->setDocumentMode(true);

    auto *resultBox = new QGroupBox(QStringLiteral("Results / Table Data"));
    auto *resultLayout = new QVBoxLayout(resultBox);
    resultLayout->setContentsMargins(6, 6, 6, 6);

    // Shared CRUD toolbar (acts on the active tab; enabled only for browse tabs).
    auto *crudRow = new QHBoxLayout;
    m_browseLabel = new QLabel(QStringLiteral("Query results (read-only)"));
    m_browseLabel->setObjectName(QStringLiteral("browseLabel"));
    m_browseLabel->setStyleSheet(QStringLiteral("color:#666; font-style: italic;"));
    crudRow->addWidget(m_browseLabel);
    crudRow->addStretch();
    m_addRowButton = new QPushButton(QStringLiteral("+ Add Row"));
    m_addRowButton->setObjectName(QStringLiteral("insertRowButton"));
    m_deleteRowButton = new QPushButton(QStringLiteral("\u2212 Delete Row"));
    m_deleteRowButton->setObjectName(QStringLiteral("deleteRowButton"));
    m_saveButton = new QPushButton(QStringLiteral("\u2714 Save"));
    m_saveButton->setObjectName(QStringLiteral("saveChangesButton"));
    m_revertButton = new QPushButton(QStringLiteral("\u27F2 Revert"));
    m_revertButton->setObjectName(QStringLiteral("revertButton"));
    for (QPushButton *b : {m_addRowButton, m_deleteRowButton, m_saveButton, m_revertButton}) {
        b->setEnabled(false);
        crudRow->addWidget(b);
    }

    // Web dashboard: a pin toggle for the active result + a bold green launcher.
    m_dashboard = new core::DashboardServer(this);
    m_pinCheck = new QCheckBox(QStringLiteral("\U0001F4CC Panoya Ekle"));
    m_pinCheck->setObjectName(QStringLiteral("pinDashboardCheck"));
    m_pinCheck->setToolTip(QStringLiteral("Bu sonucu web dashboard'ina sabitle / kaldir"));
    m_pinCheck->setEnabled(false);
    crudRow->addSpacing(10);
    crudRow->addWidget(m_pinCheck);
    m_dashboardButton = new QPushButton(QStringLiteral("\U0001F7E2 Dashboard Panel"));
    m_dashboardButton->setObjectName(QStringLiteral("dashboardButton"));
    m_dashboardButton->setToolTip(QStringLiteral("Yerel (localhost) web dashboard'unu tarayicida ac"));
    m_dashboardButton->setStyleSheet(QStringLiteral(
        "QPushButton{background:#2e7d32;color:#ffffff;font-weight:bold;"
        "border:none;border-radius:6px;padding:6px 14px;}"
        "QPushButton:hover{background:#1b5e20;}"
        "QPushButton:pressed{background:#14501a;}"));
    crudRow->addWidget(m_dashboardButton);

    resultLayout->addLayout(crudRow);
    resultLayout->addWidget(m_resultTabs);

    // Start with a single empty read-only tab so there is always a grid.
    auto *initialTab = new ResultTab;
    attachResultTab(initialTab);
    m_resultTabs->addTab(initialTab, QStringLiteral("Result"));

    auto *rightSplit = new QSplitter(Qt::Vertical);
    rightSplit->addWidget(resultBox);  // top: data grid
    rightSplit->addWidget(editorBox);  // bottom: SQL editor
    rightSplit->setSizes({420, 280});

    // Center panel as tabs: SQL & Data (existing) + ER Diagram (FK view).
    m_centerTabs = new QTabWidget;
    m_centerTabs->setObjectName(QStringLiteral("rightTabs"));
    m_centerTabs->addTab(rightSplit, QStringLiteral("SQL & Data"));
    m_erView = new ErDiagramView(&m_db);
    m_erView->setObjectName(QStringLiteral("erView"));
    m_centerTabs->addTab(m_erView, QStringLiteral("ER Diagram"));
    connect(m_centerTabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (index == 1 && m_db.isConnected())
            m_erView->rebuild();
    });

    // Right-hand AI assistant dock (Chat + Analytics).
    m_aiPanel = new AiAssistantPanel(&m_db);
    m_aiPanel->setObjectName(QStringLiteral("aiPanel"));
    connect(m_aiPanel, &AiAssistantPanel::insertSqlRequested,
            this, &MainWindow::onAiInsertSql);
    connect(m_aiPanel, &AiAssistantPanel::presentResult,
            this, &MainWindow::onAiPresentResult);
    connect(m_aiPanel->orchestrator()->ollama(), &ai::OllamaClient::availabilityChecked,
            this, &MainWindow::updateAiBadge);

    auto *centralSplit = new QSplitter(Qt::Horizontal);
    centralSplit->addWidget(sidebar);
    centralSplit->addWidget(m_centerTabs);
    centralSplit->addWidget(m_aiPanel);
    centralSplit->setStretchFactor(0, 0);
    centralSplit->setStretchFactor(1, 1);
    centralSplit->setStretchFactor(2, 0);
    centralSplit->setCollapsible(0, true);
    centralSplit->setCollapsible(1, false);
    centralSplit->setCollapsible(2, true);
    sidebar->setMinimumWidth(160);
    m_aiPanel->setMinimumWidth(240);
    centralSplit->setSizes({260, 660, 320});
    setCentralWidget(centralSplit);

    // --- Status bar ------------------------------------------------------
    m_connectionLabel = new QLabel(QStringLiteral("Disconnected"));
    m_connectionLabel->setObjectName(QStringLiteral("connectionLabel"));
    statusBar()->addPermanentWidget(m_connectionLabel);

    m_aiBadge = new QLabel(QStringLiteral("AI: hazırlanıyor…"));
    m_aiBadge->setObjectName(QStringLiteral("aiBadge"));
    m_aiBadge->setStyleSheet(QStringLiteral("color:#666;"));
    statusBar()->addPermanentWidget(m_aiBadge);

    showStatusMessage(QStringLiteral("Welcome. Create a new database or open an existing one from the File menu."));

    // --- Wiring ----------------------------------------------------------
    connect(m_executeButton, &QPushButton::clicked, this, &MainWindow::executeSql);
    connect(m_editor, &QueryEditor::executeRequested, this, &MainWindow::executeSql);
    connect(m_tree, &QTreeView::doubleClicked, this, &MainWindow::onTreeDoubleClicked);
    connect(m_tree, &QTreeView::customContextMenuRequested, this, &MainWindow::onTreeContextMenu);

    // Single-clicking a table (or one of its columns) opens/reuses a browse
    // tab in the results area, without touching the editor contents.
    connect(m_tree->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this](const QItemSelection &selected, const QItemSelection &) {
                if (selected.indexes().isEmpty())
                    return;
                const QString table = m_treeModel->tableForIndex(selected.indexes().first());
                if (!table.isEmpty())
                    openTableTab(table);
            });

    // Grid CRUD buttons act on the active tab; per-tab views own the context menu.
    connect(m_addRowButton, &QPushButton::clicked, this, &MainWindow::addRowToTable);
    connect(m_deleteRowButton, &QPushButton::clicked, this, &MainWindow::deleteCurrentRow);
    connect(m_saveButton, &QPushButton::clicked, this, &MainWindow::saveTableChanges);
    connect(m_revertButton, &QPushButton::clicked, this, &MainWindow::revertTableChanges);
    connect(m_pinCheck, &QCheckBox::toggled, this, &MainWindow::onPinToggled);
    connect(m_dashboardButton, &QPushButton::clicked, this, &MainWindow::openDashboard);
    connect(m_resultTabs, &QTabWidget::currentChanged, this, &MainWindow::updateActiveTabChrome);
    connect(m_resultTabs, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);
    updateActiveTabChrome();
}

void MainWindow::setupActions()
{
    m_actNew = new QAction(QStringLiteral("&New Database..."), this);
    m_actNew->setShortcut(QKeySequence::New);
    connect(m_actNew, &QAction::triggered, this, &MainWindow::newDatabase);

    m_actOpen = new QAction(QStringLiteral("&Open Database..."), this);
    m_actOpen->setShortcut(QKeySequence::Open);
    connect(m_actOpen, &QAction::triggered, this, &MainWindow::openDatabase);

    m_actClose = new QAction(QStringLiteral("&Close Database"), this);
    connect(m_actClose, &QAction::triggered, this, &MainWindow::closeDatabase);

    m_actQuit = new QAction(QStringLiteral("E&xit"), this);
    m_actQuit->setShortcut(QKeySequence::Quit);
    connect(m_actQuit, &QAction::triggered, qApp, &QApplication::quit);

    m_actRefreshSchema = new QAction(QStringLiteral("Refresh &Schema"), this);
    m_actRefreshSchema->setShortcut(QKeySequence(QStringLiteral("F5")));
    connect(m_actRefreshSchema, &QAction::triggered, this, &MainWindow::refreshSchema);

    m_actCreateTable = new QAction(QStringLiteral("&Create Table..."), this);
    connect(m_actCreateTable, &QAction::triggered, this, &MainWindow::createTableViaDialog);

    m_actAddColumn = new QAction(QStringLiteral("Add &Column..."), this);
    connect(m_actAddColumn, &QAction::triggered, this, &MainWindow::addColumnViaDialog);

    m_actDropTable = new QAction(QStringLiteral("&Drop Table"), this);
    connect(m_actDropTable, &QAction::triggered, this, &MainWindow::dropSelectedTable);

    m_actExport = new QAction(QStringLiteral("E&xport Result…"), this);
    m_actExport->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+E")));
    connect(m_actExport, &QAction::triggered, this, &MainWindow::exportActiveResult);

    m_actExportTable = new QAction(QStringLiteral("Export &Table…"), this);
    connect(m_actExportTable, &QAction::triggered, this, &MainWindow::exportSelectedTable);

    m_actImport = new QAction(QStringLiteral("&Import File…"), this);
    m_actImport->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+I")));
    connect(m_actImport, &QAction::triggered, this, &MainWindow::importFile);

    m_actExecute = new QAction(QStringLiteral("&Execute"), this);
    m_actExecute->setShortcut(QKeySequence(QStringLiteral("Ctrl+Return")));
    connect(m_actExecute, &QAction::triggered, this, &MainWindow::executeSql);

    m_actExplain = new QAction(QStringLiteral("E&xplain Plan"), this);
    m_actExplain->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+X")));
    connect(m_actExplain, &QAction::triggered, this, &MainWindow::explainCurrentQuery);

    m_actFormat = new QAction(QStringLiteral("&Format SQL"), this);
    m_actFormat->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+F")));
    connect(m_actFormat, &QAction::triggered, this, &MainWindow::formatCurrentSql);

    // Single unified menu bar: all commands live in the menus (no toolbar).
    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    fileMenu->addAction(m_actNew);
    fileMenu->addAction(m_actOpen);
    fileMenu->addSeparator();
    QAction *actConnMgr = fileMenu->addAction(QStringLiteral("Connection Manager\u2026"));
    actConnMgr->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+C")));
    connect(actConnMgr, &QAction::triggered, this, &MainWindow::openConnectionDialog);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actImport);
    fileMenu->addAction(m_actExport);
    fileMenu->addAction(m_actExportTable);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actClose);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actQuit);

    QMenu *tableMenu = menuBar()->addMenu(QStringLiteral("&Table"));
    tableMenu->addAction(m_actCreateTable);
    tableMenu->addAction(m_actAddColumn);
    tableMenu->addAction(m_actDropTable);
    tableMenu->addSeparator();
    tableMenu->addAction(QStringLiteral("Create &View\u2026"), this, &MainWindow::createViewViaDialog);
    tableMenu->addAction(QStringLiteral("Create &Index\u2026"), this, [this] {
        const QString table = selectedTableName();
        if (table.isEmpty()) {
            showStatusMessage(QStringLiteral("Select a table in the sidebar first."), true);
            return;
        }
        createIndexViaDialogForTable(table);
    });

    QMenu *queryMenu = menuBar()->addMenu(QStringLiteral("&Query"));
    queryMenu->addAction(m_actExecute);
    queryMenu->addAction(m_actExplain);
    queryMenu->addAction(m_actFormat);
    queryMenu->addSeparator();
    queryMenu->addAction(m_actRefreshSchema);

    // View menu: AI Assistant dock toggle.
    QMenu *viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
    QAction *toggleAi = viewMenu->addAction(QStringLiteral("AI &Assistant"));
    toggleAi->setCheckable(true);
    toggleAi->setChecked(true);
    toggleAi->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+A")));
    connect(toggleAi, &QAction::toggled, this, [this](bool on) {
        if (m_aiPanel)
            m_aiPanel->setVisible(on);
    });

    QAction *toggleDark = viewMenu->addAction(QStringLiteral("&Dark Mode"));
    toggleDark->setCheckable(true);
    toggleDark->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));
    connect(toggleDark, &QAction::toggled, this, [this](bool dark) {
        Theme::apply(qApp, dark ? Theme::Mode::Dark : Theme::Mode::Light);
        if (m_editor)
            m_editor->refreshTheme();
        if (m_aiPanel)
            m_aiPanel->refreshTheme();
        if (m_erView)
            m_erView->refresh();
    });

    m_actAutoFormat = viewMenu->addAction(QStringLiteral("Auto-&Format SQL"));
    m_actAutoFormat->setCheckable(true);
    m_actAutoFormat->setChecked(QSettings(QStringLiteral("QuickDb"), QStringLiteral("QuickDb"))
                                    .value(QStringLiteral("autoFormat"), false)
                                    .toBool());
    connect(m_actAutoFormat, &QAction::toggled, this, [](bool on) {
        QSettings(QStringLiteral("QuickDb"), QStringLiteral("QuickDb"))
            .setValue(QStringLiteral("autoFormat"), on);
    });

    viewMenu->addSeparator();
    QAction *erFit = viewMenu->addAction(QStringLiteral("ER Diagram: &Fit View"));
    erFit->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+B")));
    connect(erFit, &QAction::triggered, this, [this] {
        if (m_erView) {
            m_erView->rebuild();
            m_erView->fitToContents();
        }
    });

    QMenu *helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));
    helpMenu->addAction(QStringLiteral("&About QuickDb"), this, [this] {
        QMessageBox::about(
            this, QStringLiteral("About QuickDb"),
            QStringLiteral(
                "<h3>QuickDb Pro</h3>"
                "<p>SQLite &amp; PostgreSQL management with a local AI "
                "assistant: natural-language queries, charts, ER diagrams "
                "and business-analyst insights \u2014 all offline.</p>"));
    });
    helpMenu->addAction(QStringLiteral("&Keyboard Shortcuts"), this, [this] {
        QMessageBox::information(
            this, QStringLiteral("Keyboard Shortcuts"),
            QStringLiteral(
                "Ctrl+Enter \u2014 Execute SQL\n"
                "Ctrl+Shift+F \u2014 Format SQL\n"
                "Ctrl+Shift+X \u2014 Explain Plan\n"
                "F5 \u2014 Refresh Schema\n"
                "Ctrl+Shift+C \u2014 Connection Manager\n"
                "Ctrl+Shift+I \u2014 Import File\n"
                "Ctrl+Shift+E \u2014 Export Result\n"
                "Ctrl+Shift+A \u2014 Toggle AI Assistant\n"
                "Ctrl+D \u2014 Toggle Dark Mode\n"
                "Ctrl+Shift+B \u2014 ER Diagram Fit View\n"
                "Ctrl+Space \u2014 Schema Auto-complete"));
    });
}

// ---------------------------------------------------------------------------
// Connection lifecycle
// ---------------------------------------------------------------------------

void MainWindow::newDatabase()
{
    qDebug() << "Button clicked: New Database";
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Create New SQLite Database"),
        QDir::homePath() + QStringLiteral("/untitled.db"),
        QStringLiteral("SQLite Database (*.db *.sqlite *.sqlite3);;All Files (*)"),
        nullptr, QFileDialog::DontUseNativeDialog); // Qt dialog: always foreground, instant
    if (path.isEmpty())
        return;
    connectToPath(path, /*create=*/true);
}

void MainWindow::openDatabase()
{
    qDebug() << "Button clicked: Open Database";
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open SQLite Database"), QDir::homePath(),
        QStringLiteral("SQLite Database (*.db *.sqlite *.sqlite3);;All Files (*)"),
        nullptr, QFileDialog::DontUseNativeDialog); // Qt dialog: always foreground, instant
    if (path.isEmpty())
        return;
    connectToPath(path, /*create=*/false);
}

void MainWindow::connectToPath(const QString &filePath, bool create)
{
    qDebug() << "connectToPath:" << filePath << "create=" << create;
    // Result/table models must release their queries before the
    // connection goes away.
    closeAllTabs();

    QString error;
    const bool ok = create ? m_db.createDatabase(filePath, &error)
                           : m_db.openDatabase(filePath, &error);
    if (!ok) {
        QMessageBox::critical(this,
                              QStringLiteral("Connection Failed"),
                              QStringLiteral("Could not %1 database:\n%2\n%3")
                                  .arg(create ? QStringLiteral("create")
                                              : QStringLiteral("open"),
                                       filePath, error));
        showStatusMessage(QStringLiteral("Connection failed: %1").arg(error), true);
        updateConnectionStatus();
        refreshSchema();
        return;
    }

    updateConnectionStatus();
    refreshSchema();
    showStatusMessage(QStringLiteral("%1 database '%2' successfully.")
                          .arg(create ? QStringLiteral("Created") : QStringLiteral("Opened"),
                               QFileInfo(filePath).fileName()));
}

void MainWindow::closeDatabase()
{
    closeAllTabs();
    m_db.closeDatabase();
    updateConnectionStatus();
    refreshSchema();
    showStatusMessage(QStringLiteral("Database connection closed."));
}

void MainWindow::openConnectionDialog()
{
    ConnectionDialog dialog(m_db.isConnected() ? m_db.currentConfig()
                                               : core::ConnectionConfig{}, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const core::ConnectionConfig cfg = dialog.config();
    closeAllTabs(); // release any models bound to the previous connection
    QString error;
    if (!m_db.connect(cfg, &error)) {
        QMessageBox::critical(this, QStringLiteral("Connection Failed"),
                              QStringLiteral("Could not connect '%1':\n%2")
                                  .arg(cfg.title.isEmpty() ? cfg.driverType : cfg.title, error));
        showStatusMessage(QStringLiteral("Connection failed: %1").arg(error), true);
        updateConnectionStatus();
        refreshSchema();
        return;
    }
    updateConnectionStatus();
    refreshSchema();
    showStatusMessage(QStringLiteral("\u2714 Connected '%1' (%2).")
                          .arg(cfg.title.isEmpty() ? m_db.currentFilePath() : cfg.title,
                               cfg.driverType));
}

void MainWindow::updateConnectionStatus()
{
    if (m_db.isConnected()) {
        const core::ConnectionConfig cfg = m_db.currentConfig();
        QString display = m_db.currentFilePath();
        if (cfg.isPostgres())
            display = QStringLiteral("postgres://%1:%2/%3")
                          .arg(cfg.host.isEmpty() ? QStringLiteral("localhost") : cfg.host)
                          .arg(cfg.port > 0 ? cfg.port : 5432)
                          .arg(cfg.databaseName);
        m_connectionLabel->setText(QStringLiteral("● Connected: %1").arg(display));
        m_connectionLabel->setStyleSheet(QStringLiteral("color: #2e7d32; font-weight: bold;"));
    } else {
        m_connectionLabel->setText(QStringLiteral("● Disconnected"));
        m_connectionLabel->setStyleSheet(QStringLiteral("color: #b71c1c; font-weight: bold;"));
    }
}

// ---------------------------------------------------------------------------
// Query execution
// ---------------------------------------------------------------------------

void MainWindow::executeSql()
{
    qDebug() << "Button clicked: Execute";
    if (!m_db.isConnected()) {
        showStatusMessage(QStringLiteral("No database connected. Use File \u25b8 New/Open Database first."), true);
        return;
    }

    // View \u25b8 Auto-Format SQL: silently prettify the full document first
    // (never touches an explicit selection).
    if (m_actAutoFormat && m_actAutoFormat->isChecked() &&
        m_editor->textCursor().selectedText().trimmed().isEmpty())
        m_editor->formatSql();

    const QString sql = m_editor->statementToExecute().trimmed();
    if (sql.isEmpty()) {
        showStatusMessage(QStringLiteral("Nothing to execute."), true);
        return;
    }

    core::QueryResult result = m_db.runScript(sql);

    if (!result.success) {
        showStatusMessage(QStringLiteral("\u2716 %1").arg(result.errorMessage), true);
        return;
    }

    // Any executed SELECT opens in a fresh read-only result tab so multiple
    // queries can live side by side. Non-resultset statements just report.
    if (result.hasResultSet) {
        openQueryTab(result, sql);
        int rows = 0;
        if (ResultTab *t = activeResultTab())
            if (QAbstractItemModel *m = t->model())
                rows = m->rowCount();
        showStatusMessage(QStringLiteral("\u2714 %1 row(s) returned in %2 ms.")
                              .arg(rows).arg(result.elapsedMs));
    } else {
        showStatusMessage(QStringLiteral("\u2714 Executed successfully (%1 row(s) affected) in %2 ms.")
                              .arg(result.affectedRows).arg(result.elapsedMs));
    }

    // DDL may change the schema; keep the sidebar in sync.
    const QString lower = sql.toLower();
    if (lower.contains(QStringLiteral("create table")) ||
        lower.contains(QStringLiteral("drop table")) ||
        lower.contains(QStringLiteral("alter table")))
        refreshSchema();
}

void MainWindow::explainCurrentQuery()
{
    if (!m_db.isConnected()) {
        showStatusMessage(QStringLiteral("No database connected."), true);
        return;
    }
    const QString sql = m_editor->statementToExecute().trimmed();
    if (sql.isEmpty()) {
        showStatusMessage(QStringLiteral("Nothing to explain."), true);
        return;
    }
    core::QueryResult plan = m_db.explainQueryPlan(sql);
    if (!plan.success || !plan.hasResultSet) {
        showStatusMessage(QStringLiteral("\u2716 Explain failed: %1").arg(plan.errorMessage), true);
        return;
    }
    openQueryTab(plan, QStringLiteral("EXPLAIN QUERY PLAN ") + sql);
    showStatusMessage(QStringLiteral("\u2714 Query plan shown in a new tab."));
}

void MainWindow::formatCurrentSql()
{
    if (m_editor)
        m_editor->formatSql();
}

// ---------------------------------------------------------------------------
// Schema sidebar
// ---------------------------------------------------------------------------

void MainWindow::refreshSchema()
{
    qDebug() << "Slot: refreshSchema";
    m_treeModel->refresh();
    if (m_db.isConnected())
        m_tree->expandToDepth(1); // reveal tables/rows under their categories
    if (m_erView)
        m_erView->rebuild();

    // Feed the SQL editor's auto-completion with table names + table.column.
    if (m_editor) {
        QStringList tokens;
        for (const QString &t : m_db.tableNames()) {
            tokens << t;
            for (const core::ColumnInfo &c : m_db.columnsOf(t))
                tokens << QStringLiteral("%1.%2").arg(t, c.name);
        }
        m_editor->setSchemaCompletions(tokens);
    }
}

QString MainWindow::selectedTableName() const
{
    return m_treeModel->tableForIndex(m_tree->currentIndex());
}

void MainWindow::attachResultTab(ResultTab *tab)
{
    if (!tab)
        return;
    QTableView *view = tab->view();
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(view, &QTableView::customContextMenuRequested,
            this, &MainWindow::onResultContextMenu);
}

int MainWindow::addResultTab(ResultTab *tab, const QString &title)
{
    // Recycle the empty placeholder tab (no table, no SQL) if it is the only
    // such tab, so opening the first real result doesn't leave a blank "Result".
    QWidget *cur = m_resultTabs->currentWidget();
    auto *curTab = qobject_cast<ResultTab *>(cur);
    const bool curIsEmpty = curTab && curTab != tab
                            && curTab->table().isEmpty() && curTab->sql().isEmpty();
    const int idx = m_resultTabs->addTab(tab, title);
    m_resultTabs->setCurrentIndex(idx);
    if (curIsEmpty && m_resultTabs->count() > 1) {
        m_resultTabs->removeTab(m_resultTabs->indexOf(curTab));
        curTab->releaseData();
        curTab->deleteLater();
    }
    return m_resultTabs->indexOf(tab);
}

ResultTab *MainWindow::activeResultTab() const
{
    if (!m_resultTabs)
        return nullptr;
    return qobject_cast<ResultTab *>(m_resultTabs->currentWidget());
}

QTableView *MainWindow::currentResultView() const
{
    if (const ResultTab *tab = activeResultTab())
        return tab->view();
    return nullptr;
}

void MainWindow::updateActiveTabChrome()
{
    ResultTab *tab = activeResultTab();
    const bool editable = tab && tab->isBrowseEditable();
    setBrowseControlsEnabled(editable);
    if (m_browseLabel) {
        if (editable)
            m_browseLabel->setText(QStringLiteral("Browsing: %1 (editable)").arg(tab->table()));
        else
            m_browseLabel->setText(QStringLiteral("Query results (read-only)"));
    }
    // Feed the AI Analytics tab from the active read-only query model.
    if (m_aiPanel && tab && tab->mode() == ResultTab::Mode::Query) {
        if (auto *qm = qobject_cast<QSqlQueryModel *>(tab->model()))
            m_aiPanel->showResultFromModel(qm);
    }
    syncPinCheckbox();
}

void MainWindow::syncPinCheckbox()
{
    if (!m_pinCheck)
        return;
    ResultTab *tab = activeResultTab();
    QStringList cols;
    QVector<QVector<QString>> rows;
    const bool hasData = collectTableData(tab, &cols, &rows) && !rows.isEmpty();
    const QString pinnedId = tab ? tab->property("dashboardId").toString() : QString();

    QSignalBlocker blocker(m_pinCheck);
    m_pinCheck->setEnabled(hasData);
    m_pinCheck->setChecked(!pinnedId.isEmpty());
    m_pinCheck->setToolTip(hasData
        ? QStringLiteral("Bu sonucu web dashboard'ina sabitle / kaldir")
        : QStringLiteral("Sabitlemek icin oncelikle bir sonuc calistirin"));
}

bool MainWindow::collectTableData(ResultTab *tab, QStringList *columns,
                                  QVector<QVector<QString>> *rows) const
{
    if (!tab || !columns || !rows)
        return false;
    QAbstractItemModel *model = tab->model();
    if (!model || model->columnCount() == 0) {
        columns->clear();
        rows->clear();
        return false;
    }
    columns->clear();
    for (int c = 0; c < model->columnCount(); ++c)
        *columns << model->headerData(c, Qt::Horizontal).toString();
    rows->clear();
    for (int r = 0; r < model->rowCount(); ++r) {
        QVector<QString> row;
        for (int c = 0; c < model->columnCount(); ++c)
            row << model->index(r, c).data(Qt::DisplayRole).toString();
        *rows << row;
    }
    return true;
}

void MainWindow::openDashboard()
{
    if (!m_dashboard->start()) {
        showStatusMessage(QStringLiteral("Dashboard sunucusu baslatilamadi."), true);
        return;
    }
    const QString url = m_dashboard->baseUrl();
    QDesktopServices::openUrl(QUrl(url));
    if (m_dashboard->isEmpty())
        showStatusMessage(QStringLiteral("Dashboard acildi: %1 — sol ustekti “Panoya Ekle” ile sonuc sabitleyin.").arg(url));
    else
        showStatusMessage(QStringLiteral("Dashboard acildi: %1 (%2 panel)").arg(url).arg(m_dashboard->panelCount()));
}

void MainWindow::onPinToggled(bool on)
{
    ResultTab *tab = activeResultTab();
    if (!tab)
        return;

    if (!on) {
        const QString id = tab->property("dashboardId").toString();
        if (!id.isEmpty())
            m_dashboard->removePanel(id);
        tab->setProperty("dashboardId", QVariant());
        showStatusMessage(QStringLiteral("Panel dashboard'dan kaldirildi."));
        return;
    }

    QStringList cols;
    QVector<QVector<QString>> rows;
    if (!collectTableData(tab, &cols, &rows) || rows.isEmpty()) {
        QSignalBlocker blocker(m_pinCheck);
        m_pinCheck->setChecked(false);
        showStatusMessage(QStringLiteral("Sabitlenecek veri yok — oncelikle bir sorgu calistirin."), true);
        return;
    }

    // Title is auto-generated from the query (e.g. the main table name).
    const QString id = m_dashboard->addPanel(QString(), tab->sql(), cols, rows);
    tab->setProperty("dashboardId", id);
    const QString title = m_dashboard->panelTitle(id);
    showStatusMessage(QStringLiteral("\u2714 “%1” dashboard'a otomatik eklendi — yeşil butonla açabilirsiniz.").arg(title));
}

void MainWindow::onTabCloseRequested(int index)
{
    auto *tab = qobject_cast<ResultTab *>(m_resultTabs->widget(index));
    if (tab && tab->isDirty()) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Close Tab"),
            QStringLiteral("This tab has unsaved changes. Close anyway?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }
    if (tab) {
        tab->releaseData();
        m_resultTabs->removeTab(index);
        tab->deleteLater();
    } else {
        m_resultTabs->removeTab(index);
    }
    // Always keep at least one (empty) grid tab available.
    if (m_resultTabs->count() == 0) {
        auto *initialTab = new ResultTab;
        attachResultTab(initialTab);
        m_resultTabs->addTab(initialTab, QStringLiteral("Result"));
    }
    updateActiveTabChrome();
}

void MainWindow::openTableTab(const QString &table)
{
    if (!m_db.isConnected() || table.isEmpty())
        return;

    // Reuse an existing browse tab for this table (picks up external changes).
    for (int i = 0; i < m_resultTabs->count(); ++i) {
        auto *t = qobject_cast<ResultTab *>(m_resultTabs->widget(i));
        if (t && t->mode() == ResultTab::Mode::Browse && t->table() == table) {
            m_resultTabs->setCurrentIndex(i);
            t->refreshView();
            return;
        }
    }

    auto *tab = new ResultTab;
    attachResultTab(tab);
    tab->browseTable(table, m_db.database());
    addResultTab(tab, table);
    updateActiveTabChrome();
    showStatusMessage(QStringLiteral("\u2714 Browsing '%1' \u2014 edit cells / add / delete rows, "
                                     "then click \u2714 Save to commit.")
                          .arg(table));
}

void MainWindow::openQueryTab(core::QueryResult &result, const QString &sql)
{
    if (!result.hasResultSet)
        return;

    QString snippet = sql.simplified();
    if (snippet.size() > 28)
        snippet = snippet.left(28) + QStringLiteral("\u2026");

    auto *tab = new ResultTab;
    attachResultTab(tab);
    tab->runQuery(result, m_db.database(), sql);
    addResultTab(tab, QStringLiteral("Query: %1").arg(snippet));
    updateActiveTabChrome();
}

void MainWindow::openSnapshotTab(const ai::TableData &data, const QString &sql)
{
    if (data.isEmpty())
        return;
    QString snippet = sql.simplified();
    if (snippet.size() > 28)
        snippet = snippet.left(28) + QStringLiteral("\u2026");

    auto *tab = new ResultTab;
    attachResultTab(tab);
    tab->showSnapshot(data.columns, data.rows, sql);
    addResultTab(tab, QStringLiteral("AI: %1").arg(snippet));
    updateActiveTabChrome();
}

void MainWindow::closeAllTabs()
{
    if (!m_resultTabs)
        return;
    while (m_resultTabs->count() > 0) {
        QWidget *w = m_resultTabs->widget(0);
        m_resultTabs->removeTab(0);
        if (auto *t = qobject_cast<ResultTab *>(w)) {
            t->releaseData();
            t->deleteLater();
        } else {
            w->deleteLater();
        }
    }
    auto *initialTab = new ResultTab;
    attachResultTab(initialTab);
    m_resultTabs->addTab(initialTab, QStringLiteral("Result"));
    updateActiveTabChrome();
}

void MainWindow::setBrowseControlsEnabled(bool enabled)
{
    const QList<QPushButton *> buttons = {m_addRowButton, m_deleteRowButton,
                                          m_saveButton, m_revertButton};
    for (QPushButton *b : buttons) {
        if (b)
            b->setEnabled(enabled);
    }
}

void MainWindow::addRowToTable()
{
    ResultTab *tab = activeResultTab();
    if (!tab || !tab->isBrowseEditable())
        return;
    if (tab->addRow())
        showStatusMessage(QStringLiteral("New row appended (staged) \u2014 fill the cells, "
                                         "then click \u2714 Save."));
}

void MainWindow::deleteCurrentRow()
{
    ResultTab *tab = activeResultTab();
    if (!tab || !tab->isBrowseEditable())
        return;
    if (!tab->deleteCurrentRow()) {
        showStatusMessage(QStringLiteral("Select a row in the grid first."), true);
        return;
    }
    showStatusMessage(QStringLiteral("Row marked for deletion (staged) \u2014 "
                                     "click \u2714 Save to commit."));
}

void MainWindow::saveTableChanges()
{
    ResultTab *tab = activeResultTab();
    if (!tab || !tab->isBrowseEditable())
        return;
    const QString err = tab->save();
    if (err.isEmpty()) {
        int rows = 0;
        if (QAbstractItemModel *m = tab->model())
            rows = m->rowCount();
        showStatusMessage(QStringLiteral("\u2714 Changes saved to '%1' (%2 row(s) now).")
                              .arg(tab->table()).arg(rows));
    } else {
        showStatusMessage(QStringLiteral("\u2716 Save failed: %1").arg(err), true);
    }
}

void MainWindow::revertTableChanges()
{
    ResultTab *tab = activeResultTab();
    if (!tab)
        return;
    tab->revert();
    showStatusMessage(QStringLiteral("Staged changes discarded."));
}

// ---------------------------------------------------------------------------
// Import / Export
// ---------------------------------------------------------------------------

bool MainWindow::collectExportData(ResultTab *tab, core::io::ExportInput *out) const
{
    if (!tab || !out)
        return false;
    QAbstractItemModel *model = tab->model();
    if (!model || model->columnCount() == 0)
        return false;

    out->columns.clear();
    for (int c = 0; c < model->columnCount(); ++c)
        out->columns << model->headerData(c, Qt::Horizontal).toString();

    out->rows.clear();
    for (int r = 0; r < model->rowCount(); ++r) {
        QVector<QString> row;
        for (int c = 0; c < model->columnCount(); ++c)
            row << model->index(r, c).data(Qt::DisplayRole).toString();
        out->rows << row;
    }
    out->tableName = tab->table();
    return true;
}

void MainWindow::exportActiveResult()
{
    ResultTab *tab = activeResultTab();
    core::io::ExportInput in;
    if (!collectExportData(tab, &in)) {
        showStatusMessage(QStringLiteral("Nothing to export \u2014 run a query or open a table first."), true);
        return;
    }
    ExportDialog dlg(in, this);
    if (dlg.exec() == QDialog::Accepted) {
        const QString err = dlg.writeTo();
        if (err.isEmpty())
            showStatusMessage(QStringLiteral("\u2714 Exported %1 row(s) to %2")
                                  .arg(in.rows.size()).arg(dlg.filePath()));
        else
            showStatusMessage(QStringLiteral("\u2716 Export failed: %1").arg(err), true);
    }
}

void MainWindow::exportSelectedTable()
{
    const QString table = selectedTableName();
    if (table.isEmpty()) {
        showStatusMessage(QStringLiteral("Select a table in the sidebar first."), true);
        return;
    }
    core::QueryResult res = m_db.runSingle(QStringLiteral("SELECT * FROM %1")
                                               .arg(core::DatabaseManager::quoteIdentifier(table)));
    if (!res.hasResultSet) {
        showStatusMessage(QStringLiteral("\u2716 Could not read table: %1").arg(res.errorMessage), true);
        return;
    }
    core::io::ExportInput in;
    in.tableName = table;
    const QSqlRecord record = res.query.record();
    for (int c = 0; c < record.count(); ++c)
        in.columns << record.fieldName(c);
    while (res.query.next()) {
        QVector<QString> row;
        for (int c = 0; c < record.count(); ++c)
            row << res.query.value(c).toString();
        in.rows << row;
    }
    ExportDialog dlg(in, this);
    if (dlg.exec() == QDialog::Accepted) {
        const QString err = dlg.writeTo();
        if (err.isEmpty())
            showStatusMessage(QStringLiteral("\u2714 Exported table '%1' (%2 row(s))").arg(table).arg(in.rows.size()));
        else
            showStatusMessage(QStringLiteral("\u2716 Export failed: %1").arg(err), true);
    }
}

void MainWindow::importFile()
{
    if (!m_db.isConnected()) {
        showStatusMessage(QStringLiteral("No database connected. Use File \u25B8 New/Open Database first."), true);
        return;
    }
    ImportDialog dlg(&m_db, this);
    dlg.exec();
    refreshSchema();
}

void MainWindow::onResultContextMenu(const QPoint &pos)
{
    auto *view = qobject_cast<QTableView *>(sender());
    if (!view)
        return;
    // Sync so the CRUD actions target the tab under the cursor.
    for (int i = 0; i < m_resultTabs->count(); ++i) {
        auto *t = qobject_cast<ResultTab *>(m_resultTabs->widget(i));
        if (t && t->view() == view) {
            m_resultTabs->setCurrentIndex(i);
            break;
        }
    }
    ResultTab *tab = activeResultTab();

    QMenu menu(this);
    if (tab && tab->isBrowseEditable()) {
        menu.addAction(QStringLiteral("+ Add Row"), this, &MainWindow::addRowToTable);
        menu.addAction(QStringLiteral("\u2212 Delete Row"), this, &MainWindow::deleteCurrentRow);
        menu.addSeparator();
        menu.addAction(QStringLiteral("\u2714 Save Changes"), this, &MainWindow::saveTableChanges);
        menu.addAction(QStringLiteral("\u27F2 Revert Changes"), this, &MainWindow::revertTableChanges);
        menu.addSeparator();
    }
    const QModelIndex cur = view->currentIndex();
    if (cur.isValid()) {
        menu.addAction(QStringLiteral("Copy Cell"), this, [cur] {
            QApplication::clipboard()->setText(cur.data(Qt::DisplayRole).toString());
        });
    }
    menu.addSeparator();
    menu.addAction(QStringLiteral("Export\u2026"), this, &MainWindow::exportActiveResult);
    menu.exec(view->viewport()->mapToGlobal(pos));
}

void MainWindow::onTreeDoubleClicked(const QModelIndex &index)
{
    const auto obj = m_treeModel->objectForIndex(index);
    if (obj.type == SchemaTreeModel::NodeType::Table)
        openTableTab(obj.name);
    else if (obj.type == SchemaTreeModel::NodeType::View)
        openViewTab(obj.name);
}

void MainWindow::onTreeContextMenu(const QPoint &pos)
{
    const QModelIndex index = m_tree->indexAt(pos);
    // Right-click should act on the object under the cursor, so sync selection.
    if (index.isValid())
        m_tree->setCurrentIndex(index);
    const auto obj = m_treeModel->objectForIndex(index);

    QMenu menu(this);
    if (obj.type == SchemaTreeModel::NodeType::Table) {
        const QString table = obj.name;
        menu.addAction(QStringLiteral("View All Rows"), this, [this, table] { openTableTab(table); });
        menu.addAction(QStringLiteral("Generate SELECT in Editor"), this, [this, table] {
            m_editor->setPlainText(QStringLiteral("SELECT * FROM %1 LIMIT 100;")
                                       .arg(core::DatabaseManager::quoteIdentifier(table)));
            m_editor->setFocus();
        });
        menu.addSeparator();
        menu.addAction(QStringLiteral("Export Table\u2026"), this, [this] {
            exportSelectedTable();
        });
        menu.addAction(QStringLiteral("New View\u2026"), this, &MainWindow::createViewViaDialog);
        menu.addAction(QStringLiteral("New Index\u2026"), this, [this, table] { createIndexViaDialogForTable(table); });
        menu.addSeparator();
        menu.addAction(m_actAddColumn);
        menu.addAction(m_actDropTable);
        menu.addSeparator();
    } else if (obj.type == SchemaTreeModel::NodeType::View) {
        const QString view = obj.name;
        menu.addAction(QStringLiteral("View Rows"), this, [this, view] { openViewTab(view); });
        menu.addAction(QStringLiteral("View DDL"), this, [this, view] { showObjectDdl(view); });
        menu.addSeparator();
        menu.addAction(QStringLiteral("Drop View"), this, &MainWindow::dropTreeObject);
        menu.addSeparator();
    } else if (obj.type == SchemaTreeModel::NodeType::Index
               || obj.type == SchemaTreeModel::NodeType::Trigger) {
        const QString name = obj.name;
        menu.addAction(QStringLiteral("View DDL"), this, [this, name] { showObjectDdl(name); });
        menu.addSeparator();
        menu.addAction(obj.type == SchemaTreeModel::NodeType::Index
                           ? QStringLiteral("Drop Index") : QStringLiteral("Drop Trigger"),
                       this, &MainWindow::dropTreeObject);
        menu.addSeparator();
    }
    menu.addAction(m_actCreateTable);
    menu.addAction(QStringLiteral("Refresh"), this, &MainWindow::refreshSchema);
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

// ---------------------------------------------------------------------------
// DDL UI hooks
// ---------------------------------------------------------------------------

void MainWindow::createTableViaDialog()
{
    qDebug() << "Button clicked: Create Table";
    if (!m_db.isConnected()) {
        showStatusMessage(QStringLiteral("Connect to a database first."), true);
        return;
    }

    CreateTableDialog dialog(m_db.currentConfig().isPostgres(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QString error;
    if (m_db.createTable(dialog.definition(), &error)) {
        refreshSchema();
        showStatusMessage(QStringLiteral("✔ Table '%1' created.").arg(dialog.definition().tableName));
    } else {
        showStatusMessage(QStringLiteral("✖ Create table failed: %1").arg(error), true);
    }
}

void MainWindow::addColumnViaDialog()
{
    qDebug() << "Button clicked: Add Column";
    if (!m_db.isConnected()) {
        showStatusMessage(QStringLiteral("Connect to a database first."), true);
        return;
    }

    const QStringList tables = m_db.tableNames();
    if (tables.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Add Column"),
                                 QStringLiteral("There are no tables yet. Create one first."));
        return;
    }

    AddColumnDialog dialog(tables, m_db.currentConfig().isPostgres(), this);
    const QString preselect = selectedTableName();
    if (!preselect.isEmpty()) {
        const int idx = tables.indexOf(preselect);
        if (idx >= 0)
            dialog.setCurrentTableIndex(idx);
    }
    if (dialog.exec() != QDialog::Accepted)
        return;

    QString error;
    if (m_db.addColumn(dialog.targetTable(), dialog.columnSpec(), &error)) {
        refreshSchema();
        showStatusMessage(QStringLiteral("✔ Column '%1' added to '%2'.")
                              .arg(dialog.columnSpec().name, dialog.targetTable()));
    } else {
        showStatusMessage(QStringLiteral("✖ Add column failed: %1").arg(error), true);
    }
}

void MainWindow::dropSelectedTable()
{
    qDebug() << "Button clicked: Drop Table";
    const QString table = selectedTableName();
    if (table.isEmpty()) {
        showStatusMessage(QStringLiteral("Select a table in the sidebar first."), true);
        return;
    }

    const auto answer = QMessageBox::question(
        this, QStringLiteral("Drop Table"),
        QStringLiteral("Permanently drop table '%1'?\nThis cannot be undone.").arg(table),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    // Close any browse tab bound to the table about to be dropped so its
    // QSqlTableModel releases the connection first.
    for (int i = m_resultTabs->count() - 1; i >= 0; --i) {
        auto *t = qobject_cast<ResultTab *>(m_resultTabs->widget(i));
        if (t && t->mode() == ResultTab::Mode::Browse && t->table() == table) {
            t->releaseData();
            m_resultTabs->removeTab(i);
            t->deleteLater();
        }
    }
    if (m_resultTabs->count() == 0) {
        auto *initialTab = new ResultTab;
        attachResultTab(initialTab);
        m_resultTabs->addTab(initialTab, QStringLiteral("Result"));
    }
    updateActiveTabChrome();
    QString error;
    if (m_db.dropTable(table, &error)) {
        refreshSchema();
        showStatusMessage(QStringLiteral("✔ Table '%1' dropped.").arg(table));
    } else {
        showStatusMessage(QStringLiteral("✖ Drop table failed: %1").arg(error), true);
    }
}

// ---------------------------------------------------------------------------
// Views / Indexes / Triggers (Phase F)
// ---------------------------------------------------------------------------

void MainWindow::openViewTab(const QString &view)
{
    if (!m_db.isConnected() || view.isEmpty())
        return;
    core::QueryResult res = m_db.runSingle(QStringLiteral("SELECT * FROM %1")
                                               .arg(core::DatabaseManager::quoteIdentifier(view)));
    if (!res.hasResultSet) {
        showStatusMessage(QStringLiteral("✖ Could not open view: %1").arg(res.errorMessage), true);
        return;
    }
    openQueryTab(res, QStringLiteral("SELECT * FROM %1").arg(core::DatabaseManager::quoteIdentifier(view)));
}

void MainWindow::createViewViaDialog()
{
    if (!m_db.isConnected()) {
        showStatusMessage(QStringLiteral("Connect to a database first."), true);
        return;
    }
    CreateViewDialog dialog(this);
    // Pre-fill the SELECT body if a table is selected.
    const QString table = selectedTableName();
    if (!table.isEmpty())
        dialog.findChild<QPlainTextEdit *>(QStringLiteral("viewSelect"))
            ->setPlainText(QStringLiteral("SELECT * FROM %1")
                               .arg(core::DatabaseManager::quoteIdentifier(table)));
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString name = dialog.viewName();
    const QString sql = dialog.selectSql();
    if (name.isEmpty() || sql.isEmpty()) {
        showStatusMessage(QStringLiteral("View name and SELECT are both required."), true);
        return;
    }
    QString error;
    if (m_db.createView(name, sql, &error)) {
        refreshSchema();
        showStatusMessage(QStringLiteral("✔ View '%1' created.").arg(name));
    } else {
        showStatusMessage(QStringLiteral("✖ Create view failed: %1").arg(error), true);
    }
}

void MainWindow::createIndexViaDialogForTable(const QString &table)
{
    if (!m_db.isConnected() || table.isEmpty()) {
        showStatusMessage(QStringLiteral("Select a table first."), true);
        return;
    }
    QStringList cols;
    for (const core::ColumnInfo &c : m_db.columnsOf(table))
        cols << c.name;
    if (cols.isEmpty()) {
        showStatusMessage(QStringLiteral("Table '%1' has no columns.").arg(table), true);
        return;
    }
    CreateIndexDialog dialog(table, cols, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QStringList selected = dialog.selectedColumns();
    if (selected.isEmpty()) {
        showStatusMessage(QStringLiteral("Select at least one column."), true);
        return;
    }
    QString error;
    if (m_db.createIndex(dialog.indexName(), table, selected, dialog.isUnique(), &error)) {
        refreshSchema();
        showStatusMessage(QStringLiteral("✔ Index created on '%1'.").arg(table));
    } else {
        showStatusMessage(QStringLiteral("✖ Create index failed: %1").arg(error), true);
    }
}

void MainWindow::showObjectDdl(const QString &objectName)
{
    const QString ddl = m_db.sqlFor(objectName);
    if (ddl.isEmpty()) {
        showStatusMessage(QStringLiteral("No DDL found for '%1'.").arg(objectName), true);
        return;
    }
    QMessageBox::information(this, QStringLiteral("DDL: %1").arg(objectName), ddl);
}

void MainWindow::dropTreeObject()
{
    const auto obj = m_treeModel->objectForIndex(m_tree->currentIndex());
    if (obj.name.isEmpty())
        return;
    const QString kind = obj.type == SchemaTreeModel::NodeType::View ? QStringLiteral("View")
                       : obj.type == SchemaTreeModel::NodeType::Index ? QStringLiteral("Index")
                       : QStringLiteral("Trigger");
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Drop %1").arg(kind),
        QStringLiteral("Drop %1 '%2'? This cannot be undone.").arg(kind, obj.name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    QString error;
    bool ok = false;
    switch (obj.type) {
    case SchemaTreeModel::NodeType::View:    ok = m_db.dropView(obj.name, &error); break;
    case SchemaTreeModel::NodeType::Index:   ok = m_db.dropIndex(obj.name, &error); break;
    case SchemaTreeModel::NodeType::Trigger: ok = m_db.dropTrigger(obj.name, &error); break;
    default: return;
    }
    if (ok) {
        refreshSchema();
        showStatusMessage(QStringLiteral("✔ %1 '%2' dropped.").arg(kind, obj.name));
    } else {
        showStatusMessage(QStringLiteral("✖ Drop %1 failed: %2").arg(kind, error), true);
    }
}

// ---------------------------------------------------------------------------
// AI assistant
// ---------------------------------------------------------------------------

void MainWindow::onAiInsertSql(const QString &sql)
{
    qDebug() << "AI insert SQL into editor";
    m_editor->setPlainText(sql);
    // Ensure the SQL & Data tab is visible and run it for immediate feedback.
    if (m_centerTabs)
        m_centerTabs->setCurrentIndex(0);
    m_editor->setFocus();
    executeSql();
}

void MainWindow::onAiPresentResult(const QString &sql, const ai::TableData &data,
                                   const QString &source, const QString &message)
{
    qDebug() << "AI present result" << source;
    // The orchestrator already tested/executed this read-only SQL; drop it into
    // the editor and render the SAME cached rows in a new tab (no re-query).
    if (!sql.isEmpty()) {
        m_editor->setPlainText(sql);
        if (m_centerTabs)
            m_centerTabs->setCurrentIndex(0);
    }
    openSnapshotTab(data, sql);
    showStatusMessage(QStringLiteral("\u2714 AI sorgusu test edildi ve sonuc grid'e ta\u015Fındı (%1 sat\u0131r).")
                          .arg(data.rows.size()) + (message.isEmpty() ? QString() : QStringLiteral(" ") + message));
}

void MainWindow::updateAiBadge(bool available, const QStringList &models)
{
    Q_UNUSED(models);
    if (!m_aiBadge)
        return;
    if (available) {
        const QString model = m_aiPanel ? m_aiPanel->orchestrator()->ollama()->model() : QString();
        m_aiBadge->setText(QStringLiteral("AI: Ollama (%1)").arg(model));
        m_aiBadge->setStyleSheet(QStringLiteral("color:#2e7d32;"));
    } else {
        m_aiBadge->setText(QStringLiteral("AI: Offline Kural Motoru"));
        m_aiBadge->setStyleSheet(QStringLiteral("color:#b26a00;"));
    }
}

// ---------------------------------------------------------------------------

void MainWindow::showStatusMessage(const QString &message, bool isError)
{
    // Errors linger longer so the user has time to read them.
    statusBar()->showMessage(message, isError ? 15000 : 8000);
}

} // namespace ui
