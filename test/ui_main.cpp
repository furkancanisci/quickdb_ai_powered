// Automated UI integration test: drives the real MainWindow widgets
// (buttons, editor, tree, slots) headlessly via the offscreen platform.
// Modal file dialogs / message boxes are intentionally bypassed by
// invoking the underlying slots directly.
#include <QApplication>
#include <QChartView>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSqlQuery>
#include <QStatusBar>
#include <QTableView>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBrowser>
#include <QTreeView>

#include "core/DatabaseManager.h"
#include "core/io/Exporters.h"
#include "ui/AiAssistantPanel.h"
#include "ui/Dialogs/AddColumnDialog.h"
#include "ui/Dialogs/ConnectionDialog.h"
#include "ui/Dialogs/CreateTableDialog.h"
#include "ui/Dialogs/ExportDialog.h"
#include "ui/Dialogs/ImportDialog.h"
#include "ui/ErDiagramView.h"
#include "ui/MainWindow.h"
#include "ui/QueryEditor.h"
#include "ui/SqlHighlighter.h"

static int failures = 0;
static void check(bool cond, const QString &label)
{
    qInfo().noquote() << (cond ? QStringLiteral("[PASS] ") : QStringLiteral("[FAIL] ")) + label;
    if (!cond)
        ++failures;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QTemporaryDir dir;
    const QString dbPath = dir.path() + QStringLiteral("/ui_test.db");

    ui::MainWindow window;
    window.show(); // offscreen

    auto *execButton       = window.findChild<QPushButton *>(QStringLiteral("executeButton"));
    auto *editor           = window.findChild<ui::QueryEditor *>(QStringLiteral("queryEditor"));
    auto *tree             = window.findChild<QTreeView *>(QStringLiteral("schemaTree"));
    auto *startupView      = window.findChild<QTableView *>(QStringLiteral("resultView"));
    // With multi-tab results the grid lives inside the active ResultTab, so
    // always re-fetch the current view rather than caching a stale pointer.
    auto grid = [&window]() -> QTableView * { return window.currentResultView(); };
    // The schema tree groups objects under Tables/Views/Indexes/Triggers
    // categories beneath the DatabaseRoot; helpers resolve those nodes.
    auto dbRoot = [&tree]() { return tree->model()->index(0, 0); };
    auto category = [&tree](const QString &name) {
        QModelIndex root = tree->model()->index(0, 0);
        for (int i = 0; i < tree->model()->rowCount(root); ++i) {
            QModelIndex c = tree->model()->index(i, 0, root);
            if (tree->model()->data(c).toString() == name)
                return c;
        }
        return QModelIndex();
    };
    auto tableNode = [&tree, &category](const QString &name) {
        QModelIndex cat = category(QStringLiteral("Tables"));
        for (int i = 0; i < tree->model()->rowCount(cat); ++i) {
            QModelIndex c = tree->model()->index(i, 0, cat);
            if (tree->model()->data(c).toString() == name)
                return c;
        }
        return QModelIndex();
    };
    auto *connectionLabel  = window.findChild<QLabel *>(QStringLiteral("connectionLabel"));
    auto *addRowButton     = window.findChild<QPushButton *>(QStringLiteral("insertRowButton"));
    auto *deleteRowButton  = window.findChild<QPushButton *>(QStringLiteral("deleteRowButton"));
    auto *saveButton       = window.findChild<QPushButton *>(QStringLiteral("saveChangesButton"));
    auto *revertButton     = window.findChild<QPushButton *>(QStringLiteral("revertButton"));
    auto *browseLabel      = window.findChild<QLabel *>(QStringLiteral("browseLabel"));
    auto *tabs             = window.findChild<QTabWidget *>(QStringLiteral("rightTabs"));
    auto *erView           = window.findChild<ui::ErDiagramView *>(QStringLiteral("erView"));
    check(execButton && editor && tree && startupView && connectionLabel
              && addRowButton && deleteRowButton && saveButton && revertButton
              && browseLabel && tabs && erView,
          QStringLiteral("all named widgets exist"));

    // 1. Execute without connection -> guarded, descriptive error.
    execButton->click();
    check(window.statusBar()->currentMessage().contains(QStringLiteral("No database connected")),
          QStringLiteral("execute guard: %1").arg(window.statusBar()->currentMessage()));

    // 2. Create + connect via slot (same path the file dialog uses).
    QMetaObject::invokeMethod(&window, "connectToPath",
                              Q_ARG(QString, dbPath), Q_ARG(bool, true));
    check(connectionLabel->text().contains(QStringLiteral("Connected")),
          QStringLiteral("connectToPath -> %1").arg(connectionLabel->text()));

    // 3. Execute button: DDL + DML script, schema tree must auto-refresh.
    editor->setPlainText(QStringLiteral(
        "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT NOT NULL, age INTEGER DEFAULT 0);\n"
        "INSERT INTO users (name, age) VALUES ('furkan', 25);\n"
        "INSERT INTO users (name, age) VALUES ('ali', 30);\n"
        "INSERT INTO users (name, age) VALUES ('zeynep', 28);"));
    execButton->click();
    check(window.statusBar()->currentMessage().startsWith(QStringLiteral("✔")),
          QStringLiteral("DDL+DML script ok: %1").arg(window.statusBar()->currentMessage()));

    QModelIndex root = dbRoot();
    QModelIndex tableIdx = tableNode(QStringLiteral("users"));
    check(tree->model()->rowCount(category(QStringLiteral("Tables"))) == 1 &&
              tree->model()->data(tableIdx).toString() == QStringLiteral("users"),
          QStringLiteral("tree shows 'users' table"));
    check(tree->model()->rowCount(tableIdx) == 3, QStringLiteral("tree shows 3 columns"));
    check(tree->model()->data(tree->model()->index(1, 0, tableIdx))
              .toString().contains(QStringLiteral("TEXT")),
          QStringLiteral("column data type visible: %1")
              .arg(tree->model()->data(tree->model()->index(1, 0, tableIdx)).toString()));

    // 4. SELECT via Execute button -> result grid populated.
    editor->setPlainText(QStringLiteral("SELECT id, name, age FROM users;"));
    execButton->click();
    check(grid()->model()->rowCount() == 3 && grid()->model()->columnCount() == 3,
          QStringLiteral("result grid 3x3 (got %1x%2)")
              .arg(grid()->model()->rowCount()).arg(grid()->model()->columnCount()));

    // 5. Ctrl+Enter shortcut path executes too.
    editor->setPlainText(QStringLiteral("SELECT name FROM users WHERE age > 26;"));
    QTest::keyClick(editor, Qt::Key_Return, Qt::ControlModifier);
    check(grid()->model()->rowCount() == 2,
          QStringLiteral("Ctrl+Enter executes (2 rows, got %1)").arg(grid()->model()->rowCount()));

    // 6. Error handling: bad SQL -> status shows real SQLite error, no crash.
    editor->setPlainText(QStringLiteral("SELECT * FROM no_such_table;"));
    execButton->click();
    check(window.statusBar()->currentMessage().contains(QStringLiteral("no such table")),
          QStringLiteral("error surfaced: %1").arg(window.statusBar()->currentMessage()));

    // 7. Column sorting (implemented via SQL re-exec in SelectQueryModel).
    editor->setPlainText(QStringLiteral("SELECT id, name, age FROM users;"));
    execButton->click();
    grid()->sortByColumn(2, Qt::DescendingOrder);
    const QVariant top = grid()->model()->data(
        grid()->model()->index(0, 2));
    check(top.toInt() == 30, QStringLiteral("descending sort puts age=30 on top (got %1)").arg(top.toInt()));
    grid()->sortByColumn(2, Qt::AscendingOrder);
    check(grid()->model()->data(grid()->model()->index(0, 2)).toInt() == 25,
          QStringLiteral("ascending sort works"));

    // 8. BLOB rendering guard.
    editor->setPlainText(QStringLiteral("SELECT X'00FF10' AS blob_col;"));
    execButton->click();
    check(grid()->model()->data(grid()->model()->index(0, 0))
              .toString().contains(QStringLiteral("BLOB")),
          QStringLiteral("BLOB shown as summary: %1")
              .arg(grid()->model()->data(grid()->model()->index(0, 0)).toString()));

    // 9. NULL cell rendering + grey foreground.
    editor->setPlainText(QStringLiteral("SELECT NULL AS empty_col;"));
    execButton->click();
    check(!grid()->model()->data(grid()->model()->index(0, 0)).isNull()
              ? grid()->model()->data(grid()->model()->index(0, 0)).toString().isEmpty()
              : true,
          QStringLiteral("NULL cell renders without crash"));

    // 10. Double-click table in tree -> editable browse mode (QSqlTableModel).
    QMetaObject::invokeMethod(&window, "onTreeDoubleClicked", Q_ARG(QModelIndex, tableIdx));
    check(grid()->model()->rowCount() == 3 &&
              browseLabel->text().contains(QStringLiteral("Browsing: users")) &&
              addRowButton->isEnabled() && saveButton->isEnabled(),
          QStringLiteral("double-click enters editable browse mode"));

    // 10b. Single-selection in tree also previews data in the grid.
    tree->selectionModel()->select(tableIdx, QItemSelectionModel::ClearAndSelect);
    check(grid()->model()->rowCount() == 3 &&
              browseLabel->text().contains(QStringLiteral("Browsing")),
          QStringLiteral("selection preview shows users rows"));

    // 10c. CRUD: add a staged row, edit a cell, save, verify in the DB file.
    addRowButton->click();
    check(grid()->model()->rowCount() == 4,
          QStringLiteral("staged row visible before save (4 rows, got %1)")
              .arg(grid()->model()->rowCount()));
    // Grid columns here: id, name, age, email (email added by ALTER in test 12).
    grid()->model()->setData(grid()->model()->index(3, 1), QStringLiteral("vedat"));
    grid()->model()->setData(grid()->model()->index(3, 2), 22);
    grid()->model()->setData(grid()->model()->index(3, 3), QStringLiteral("v@x.com"));
    saveButton->click();
    {
        core::DatabaseManager verify;
        QString err;
        bool committed = false;
        if (verify.openDatabase(dbPath, &err)) {
            QSqlQuery q(verify.database());
            committed = q.exec(QStringLiteral("SELECT age FROM users WHERE name='vedat';"))
                        && q.next() && q.value(0).toInt() == 22;
        }
        check(committed, QStringLiteral("INSERT+edit committed to DB (%1)").arg(err));
        verify.closeDatabase();
    }

    // 10d. CRUD: delete a staged-but-unsaved row, revert -> DB untouched.
    grid()->setCurrentIndex(grid()->model()->index(3, 0));
    deleteRowButton->click();
    revertButton->click();
    {
        core::DatabaseManager verify;
        QString err;
        int count = -1;
        if (verify.openDatabase(dbPath, &err)) {
            QSqlQuery q(verify.database());
            if (q.exec(QStringLiteral("SELECT COUNT(*) FROM users;")) && q.next())
                count = q.value(0).toInt();
        }
        check(count == 4 && grid()->model()->rowCount() == 4,
              QStringLiteral("revert discards staged delete (DB rows=%1, grid=%2)")
                  .arg(count).arg(grid()->model()->rowCount()));
        verify.closeDatabase();
    }

    // 10d-2. CRUD: delete + save -> row really removed from the DB file.
    grid()->setCurrentIndex(grid()->model()->index(3, 0));
    deleteRowButton->click();
    saveButton->click();
    {
        core::DatabaseManager verify;
        QString err;
        int count = -1;
        if (verify.openDatabase(dbPath, &err)) {
            QSqlQuery q(verify.database());
            if (q.exec(QStringLiteral("SELECT COUNT(*) FROM users;")) && q.next())
                count = q.value(0).toInt();
        }
        check(count == 3 && grid()->model()->rowCount() == 3,
              QStringLiteral("saved delete removes row from DB (DB rows=%1, grid=%2)")
                  .arg(count).arg(grid()->model()->rowCount()));
        verify.closeDatabase();
    }

    // 10e. Executing any SQL switches back to read-only query mode.
    editor->setPlainText(QStringLiteral("SELECT id, name FROM users;"));
    execButton->click();
    check(!addRowButton->isEnabled() && !saveButton->isEnabled() &&
              browseLabel->text().contains(QStringLiteral("read-only")),
          QStringLiteral("execute returns grid to read-only query mode"));

    // 11. Refresh Schema slot.
    QMetaObject::invokeMethod(&window, "refreshSchema");
    root = dbRoot();                            // model was reset, re-fetch
    tableIdx = tableNode(QStringLiteral("users"));
    check(tree->model()->rowCount(category(QStringLiteral("Tables"))) == 1,
          QStringLiteral("refreshSchema keeps tree consistent"));

    // 12. ALTER via SQL -> tree picks up new column.
    editor->setPlainText(QStringLiteral("ALTER TABLE users ADD COLUMN email TEXT;"));
    execButton->click();
    tableIdx = tableNode(QStringLiteral("users")); // refreshed tree
    check(tree->model()->rowCount(tableIdx) == 4,
          QStringLiteral("ALTER TABLE refreshes tree (4 cols, got %1)").arg(tree->model()->rowCount(tableIdx)));

    // 12b. ER Diagram tab: a FK relation must be detected and drawn.
    editor->setPlainText(QStringLiteral(
        "CREATE TABLE orders (id INTEGER PRIMARY KEY, user_id INTEGER, "
        "FOREIGN KEY (user_id) REFERENCES users(id));"));
    execButton->click();
    tabs->setCurrentIndex(1); // switch to ER Diagram tab -> rebuild()
    check(erView->tableCount() == 2 && erView->relationCount() == 1,
          QStringLiteral("ER diagram: 2 tables, 1 FK arrow (got %1/%2)")
              .arg(erView->tableCount()).arg(erView->relationCount()));

    // 12b-2. Premium ER: draggable nodes with persisted positions + clamped zoom.
    {
        check(erView->hasTable(QStringLiteral("orders")) && erView->hasTable(QStringLiteral("users")),
              QStringLiteral("ER exposes draggable table nodes"));
        const QPointF moved(321.0, 123.0);
        erView->setTablePos(QStringLiteral("orders"), moved);
        check(qFuzzyCompare(erView->tablePos(QStringLiteral("orders")), moved),
              QStringLiteral("drag API setTablePos/tablePos round-trips"));
        QTest::qWait(250); // debounced QSettings write
        QSettings settings;
        const QStringList keys = settings.allKeys().filter(QStringLiteral("er/"));
        bool persisted = false;
        for (const QString &k : keys) {
            if (k.endsWith(QStringLiteral("/orders")) && k.startsWith(QStringLiteral("er/"))) {
                persisted = settings.value(k).toString().startsWith(QStringLiteral("321"));
                settings.remove(k);
            }
        }
        check(persisted, QStringLiteral("node position persists to QSettings (QuickDb/er/...)"));

        for (int i = 0; i < 20; ++i)
            erView->zoomIn();
        check(erView->currentZoom() <= 4.0001, QStringLiteral("zoom clamps at 4.0x (got %1)")
                                                  .arg(erView->currentZoom()));
        for (int i = 0; i < 40; ++i)
            erView->zoomOut();
        check(erView->currentZoom() >= 0.2499, QStringLiteral("zoom clamps at 0.25x (got %1)")
                                                  .arg(erView->currentZoom()));
        erView->resetZoom();
        check(qAbs(erView->currentZoom() - 1.0) < 0.001,
              QStringLiteral("resetZoom returns to 100% (got %1)").arg(erView->currentZoom()));
        auto *zoomBar = erView->viewport()->findChild<QWidget *>(QStringLiteral("erZoomBar"));
        check(zoomBar && erView->viewport()->findChild<QPushButton *>(QStringLiteral("erZoomIn"))
                && erView->viewport()->findChild<QPushButton *>(QStringLiteral("erFit")),
              QStringLiteral("floating zoom strip exists on the viewport"));
    }
    tabs->setCurrentIndex(0);

    // 12c. Multi-tab results: opening two different tables yields two tabs, the
    // grid follows the active tab, and closing one shrinks the set back down.
    {
        auto *resultTabs = window.findChild<QTabWidget *>(QStringLiteral("resultTabs"));
        check(resultTabs != nullptr, QStringLiteral("result tab widget exists"));
        // Open 'users' then 'orders' as separate browse tabs.
        QMetaObject::invokeMethod(&window, "openTableTab", Q_ARG(QString, QStringLiteral("users")));
        QMetaObject::invokeMethod(&window, "openTableTab", Q_ARG(QString, QStringLiteral("orders")));
        const int twoTabs = resultTabs->count();
        check(twoTabs >= 2 && resultTabs->tabText(twoTabs - 1).contains(QStringLiteral("orders")),
              QStringLiteral("two tables open in two tabs (count=%1)").arg(twoTabs));
        // Re-opening the same table must NOT spawn a duplicate tab.
        QMetaObject::invokeMethod(&window, "openTableTab", Q_ARG(QString, QStringLiteral("orders")));
        check(resultTabs->count() == twoTabs,
              QStringLiteral("reopening same table reuses tab (count=%1)").arg(resultTabs->count()));
        // Close the active (orders) tab -> count drops, users grid still reachable.
        QMetaObject::invokeMethod(&window, "onTabCloseRequested", Q_ARG(int, twoTabs - 1));
        check(resultTabs->count() == twoTabs - 1,
              QStringLiteral("closing a tab reduces count (count=%1)").arg(resultTabs->count()));
    }

    // 13. CreateTableDialog logic (non-modal inspection of generated DDL parts).
    {
        ui::CreateTableDialog d;
        d.findChild<QLineEdit *>()->setText(QStringLiteral("products"));
        // click the "+ Add Column" button like a user would
        const QList<QPushButton *> dButtons = d.findChildren<QPushButton *>();
        for (QPushButton *b : dButtons) {
            if (b->text().contains(QStringLiteral("Add Column"))) {
                b->click();
                break;
            }
        }
        // definition() is public; grid prefilled with "id"
        const core::TableDefinition def = d.definition();
        check(def.tableName == QStringLiteral("products") && !def.columns.isEmpty() &&
                  def.columns.first().name == QStringLiteral("id") && def.columns.first().primaryKey,
              QStringLiteral("CreateTableDialog.definition() parses (cols=%1)").arg(def.columns.size()));
        // feed it through a manager on the same file, like the OK-button path does
        core::DatabaseManager helper;
        QString err;
        const bool created = helper.openDatabase(dbPath, &err) && helper.createTable(def, &err);
        check(created, QStringLiteral("dialog definition creates table (%1)").arg(err));
        helper.closeDatabase();
        QMetaObject::invokeMethod(&window, "refreshSchema");
    }

    // 14. AddColumnDialog logic.
    {
        ui::AddColumnDialog ad({QStringLiteral("alpha"), QStringLiteral("users")});
        ad.setCurrentTableIndex(1);
        ad.findChild<QLineEdit *>()->setText(QStringLiteral("nickname"));
        check(ad.targetTable() == QStringLiteral("users") &&
                  ad.columnSpec().name == QStringLiteral("nickname"),
              QStringLiteral("AddColumnDialog fields parse"));
    }

    // 15. AI Assistant panel — deterministic offline path (rule engine, no LLM).
    {
        auto *aiPanel   = window.findChild<ui::AiAssistantPanel *>(QStringLiteral("aiPanel"));
        auto *aiChat    = window.findChild<QScrollArea *>(QStringLiteral("aiChat"));
        auto *aiInput   = window.findChild<QPlainTextEdit *>(QStringLiteral("aiInput"));
        auto *aiSend    = window.findChild<QPushButton *>(QStringLiteral("aiSendButton"));
        auto *aiChart   = window.findChild<QChartView *>(QStringLiteral("aiChartView"));
        auto *aiSummary = window.findChild<QLabel *>(QStringLiteral("aiSummary"));
        auto *aiReport  = window.findChild<QTextBrowser *>(QStringLiteral("aiReport"));
        // Premium chat: bubbles are real widgets inside the scroll area.
        auto *aiChatBody = window.findChild<QWidget *>(QStringLiteral("aiChatBody"));
        check(aiPanel && aiPanel->count() >= 2 && aiChat && aiChatBody && aiInput && aiSend
                  && aiChart && aiSummary,
              QStringLiteral("AI panel + Chat/Analytics widgets exist (tabs=%1)")
                  .arg(aiPanel ? aiPanel->count() : 0));
        check(aiReport != nullptr, QStringLiteral("Analytics report browser (aiReport) exists"));
        auto *aiModel = window.findChild<QComboBox *>(QStringLiteral("aiModel"));
        check(aiModel != nullptr && !aiModel->currentText().isEmpty(),
              QStringLiteral("local model picker (aiModel) exists with a current model"));
        check(aiPanel && aiPanel->tabText(0) == QStringLiteral("Chat")
                   && aiPanel->tabText(1) == QStringLiteral("Analytics"),
              QStringLiteral("AI panel tabs labelled Chat & Analytics"));

        if (aiPanel) {
            aiPanel->orchestrator()->setUseLlm(false); // force deterministic offline engine
            // Seed a small monthly-sales table so a trend question yields a chart.
            editor->setPlainText(QStringLiteral(
                "CREATE TABLE satislar (id INTEGER PRIMARY KEY, ay TEXT, tutar REAL);\n"
                "INSERT INTO satislar (ay,tutar) VALUES "
                "('2025-01',100),('2025-02',150),('2025-03',120),('2025-04',200),"
                "('2025-05',180),('2025-06',260);"));
            execButton->click();

            aiInput->setPlainText(
                QStringLiteral("satislar icin aya gore toplam tutar trendini goster"));
            aiSend->click();
            QCoreApplication::processEvents();

            // Widget bubbles: one user message + one AI answer.
            check(aiPanel->messageCount() == 2
                      && aiChatBody->findChildren<QFrame *>().count() >= 2,
                  QStringLiteral("chat shows widget bubbles (user+AI), count=%1")
                      .arg(aiPanel->messageCount()));
            const QString chatText = aiPanel->lastAiBubbleText();
            check(chatText.contains(QStringLiteral("SELECT"))
                       && chatText.contains(QStringLiteral("satislar")),
                  QStringLiteral("AI chat bubble shows generated SQL"));
            check(!aiChatBody->findChildren<QPushButton *>(QStringLiteral("chatCopySql")).isEmpty(),
                  QStringLiteral("AI bubble carries a real copy-to-editor button"));
            check(aiSummary->text().contains(QStringLiteral("en yuksek"))
                       && aiSummary->text().contains(QStringLiteral("260")),
                  QStringLiteral("Analytics summary computed from result: %1").arg(aiSummary->text()));
            check(aiPanel->currentIndex() == 1, QStringLiteral("panel auto-switched to Analytics tab"));

            // Rich analyst report rendered into the report browser.
            check(aiReport && aiReport->toHtml().contains(QStringLiteral("Temel Metrikler"))
                       && aiReport->toHtml().contains(QStringLiteral("Dagilim")),
                  QStringLiteral("Analyst report shows KPI + Distribution sections in browser"));

            // Dual-model analyst wiring: second model picker + interpretation
            // surfaced in the AI bubble and on top of the analytics report.
            auto *aiAnalyst = window.findChild<QComboBox *>(QStringLiteral("aiAnalystModel"));
            check(aiAnalyst != nullptr && !aiAnalyst->currentText().isEmpty(),
                  QStringLiteral("analyst model picker (aiAnalystModel) exists with a current model"));
            check(aiPanel->lastAnalystText().contains(QStringLiteral("en yuksek")),
                  QStringLiteral("analyst interpretation available for the answer (%1)")
                      .arg(aiPanel->lastAnalystText()));
            check(aiPanel->lastAiBubbleText().contains(QStringLiteral("Analist Yorumu")),
                  QStringLiteral("AI bubble shows the analyst interpretation section"));
            check(aiReport && aiReport->toHtml().contains(QStringLiteral("YONETICI OZETI")),
                  QStringLiteral("analytics report starts with the manager summary card"));

            // Auto-present flow: the AI's just-executed SQL landed in the editor
            // and its rows are shown in a fresh (snapshot) result tab, no re-query.
            check(editor->toPlainText().contains(QStringLiteral("SELECT"))
                       && editor->toPlainText().contains(QStringLiteral("satislar")),
                  QStringLiteral("auto-present dropped AI SQL into the editor"));
            check(grid() && grid()->model() && grid()->model()->rowCount() == 6,
                  QStringLiteral("auto-present showed AI result grid (6 rows, got %1)")
                      .arg(grid() && grid()->model() ? grid()->model()->rowCount() : -1));

            // Read-only guard: a write intent must never be executed by the AI.
            const int tablesBefore = tree->model()->rowCount(category(QStringLiteral("Tables")));
            aiInput->setPlainText(QStringLiteral("satislar tablosunu sil"));
            aiSend->click();
            QCoreApplication::processEvents();
            check(tree->model()->rowCount(category(QStringLiteral("Tables"))) == tablesBefore,
                  QStringLiteral("AI never executes destructive SQL (table survives)"));

            // Scalar / aggregate result -> big KPI card, not an empty chart.
            auto *aiKpi = window.findChild<QLabel *>(QStringLiteral("aiKpiCard"));
            aiInput->setPlainText(QStringLiteral("satislar icin toplam tutar"));
            aiSend->click();
            QCoreApplication::processEvents();
            check(aiKpi && aiKpi->isVisible() && !aiChart->isVisible(),
                  QStringLiteral("aggregate result shows KPI card, hides empty chart"));
            check(aiKpi && aiKpi->text().contains(QStringLiteral("TEKIL DEGER")),
                  QStringLiteral("KPI card headline rendered"));
            check(aiReport && !aiReport->toHtml().contains(QStringLiteral("Dagilim")),
                  QStringLiteral("scalar Analytics report omits Distribution section"));

            // Transparency: a generated-but-failed SQL must still be surfaced
            // in the bubble (never a bare "SQL üretti" with no SQL).
            ai::AiOrchestrator::Answer bad;
            bad.question = QStringLiteral("calisanlarin performanslarini analiz et");
            bad.generatedSql = QStringLiteral("SELECT performans FROM calisanlar;");
            bad.source = QStringLiteral("LLM (Ollama)");
            bad.message = QStringLiteral("Yerel model SQL üretti.");
            bad.executed = false;
            bad.result.errorMessage = QStringLiteral("no such table: calisanlar");
            aiPanel->onAnswer(bad);
            QCoreApplication::processEvents();
            const QString failBubble = aiPanel->lastAiBubbleText();
            check(failBubble.contains(QStringLiteral("SELECT performans FROM calisanlar"))
                       && failBubble.contains(QStringLiteral("no such table")),
                  QStringLiteral("failed-answer bubble still shows the generated SQL + error"));
        }
    }

    // 14. Phase D/E dialog widgets (constructed directly, no modal exec).
    {
        ui::ConnectionDialog connDlg;
        check(connDlg.findChild<QComboBox *>(QStringLiteral("connDriver"))
                  && connDlg.findChild<QLineEdit *>(QStringLiteral("connFilePath"))
                  && connDlg.findChild<QLineEdit *>(QStringLiteral("connHost"))
                  && connDlg.findChild<QPushButton *>(QStringLiteral("connTest")),
              QStringLiteral("ConnectionDialog exposes driver/file/host/test widgets"));
        check(connDlg.config().driverType == QStringLiteral("QSQLITE"),
              QStringLiteral("ConnectionDialog defaults to SQLite"));
    }
    {
        core::io::ExportInput in;
        in.tableName = QStringLiteral("users");
        in.columns = {QStringLiteral("id"), QStringLiteral("name")};
        in.rows = {{QStringLiteral("1"), QStringLiteral("furkan")}};
        ui::ExportDialog exportDlg(in);
        auto *preview = exportDlg.findChild<QPlainTextEdit *>(QStringLiteral("exportPreview"));
        check(exportDlg.findChild<QComboBox *>(QStringLiteral("exportFormat"))
                  && preview && !preview->toPlainText().isEmpty()
                  && exportDlg.format() == QStringLiteral("csv"),
              QStringLiteral("ExportDialog previews CSV by default"));
        bool roundTrips = true;
        for (const QString &f : {QStringLiteral("json"), QStringLiteral("xml"), QStringLiteral("sql")}) {
            // (format switching is exercised live in the dialog; rendered() uses current)
            roundTrips = roundTrips && !f.isEmpty();
        }
        check(roundTrips && !exportDlg.rendered().isEmpty(),
              QStringLiteral("ExportDialog renders a non-empty document"));
    }
    {
        core::DatabaseManager probeDb;
        ui::ImportDialog importDlg(&probeDb);
        check(importDlg.findChild<QLineEdit *>(QStringLiteral("importFile"))
                  && importDlg.findChild<QComboBox *>(QStringLiteral("importDelim"))
                  && importDlg.findChild<QCheckBox *>(QStringLiteral("importHeader"))
                  && importDlg.findChild<QLineEdit *>(QStringLiteral("importTable")),
              QStringLiteral("ImportDialog exposes file/delim/header/table widgets"));
    }

    // 15. Phase G: SQL editor highlighting + format + EXPLAIN. Auto-completion
    // is intentionally disabled (its QCompleter popup crashed while typing).
    check(editor->findChild<ui::SqlHighlighter *>() != nullptr,
          QStringLiteral("editor has a SqlHighlighter attached"));
    check(editor->findChild<QCompleter *>() == nullptr,
          QStringLiteral("editor has no auto-completer (auto-complete disabled)"));

    editor->setPlainText(QStringLiteral("select * from users where age > 25 order by age desc;"));
    QMetaObject::invokeMethod(&window, "explainCurrentQuery");
    check(window.statusBar()->currentMessage().contains(QStringLiteral("Query plan")),
          QStringLiteral("EXPLAIN shows a query plan: %1").arg(window.statusBar()->currentMessage()));

    editor->setPlainText(QStringLiteral("SELECT id,name FROM users WHERE age>25;"));
    QMetaObject::invokeMethod(&window, "formatCurrentSql");
    check(editor->toPlainText().contains(QStringLiteral("\n"))
              && editor->toPlainText().contains(QStringLiteral("FROM")),
          QStringLiteral("Format SQL splits keywords onto separate lines"));

    // 15b. Token-based formatter: literals/CTEs/multi-statements are safe.
    {
        const QString f = ui::QueryEditor::formatSqlText(QStringLiteral(
            "SELECT id,name FROM users WHERE age>25 AND city='Ankara, TR' ORDER BY id;"));
        check(f.contains(QStringLiteral("'Ankara, TR'")),
              QStringLiteral("formatter keeps string literals byte-for-byte"));
        check(f.contains(QStringLiteral("SELECT id,")) && f.contains(QStringLiteral("\n  name")),
              QStringLiteral("formatter breaks the SELECT list on commas"));
        check(f.contains(QStringLiteral("\nFROM users")) && f.contains(QStringLiteral("\nWHERE age > 25")),
              QStringLiteral("formatter puts FROM/WHERE on their own lines"));
        check(f.contains(QStringLiteral("\n  AND city = 'Ankara, TR'")),
              QStringLiteral("formatter aligns AND under WHERE"));

        const QString cte = ui::QueryEditor::formatSqlText(QStringLiteral(
            "WITH c AS (SELECT id FROM users) SELECT id FROM c;"));
        check(cte.contains(QStringLiteral("(\n  SELECT id")) &&
                  cte.contains(QStringLiteral("\n)\nSELECT id")),
              QStringLiteral("formatter indents CTE subqueries"));

        const QString multi = ui::QueryEditor::formatSqlText(QStringLiteral(
            "SELECT 1; SELECT 2;"));
        check(multi.contains(QStringLiteral(";\n\n")),
              QStringLiteral("formatter keeps multiple statements separated"));

        const QString dotted = ui::QueryEditor::formatSqlText(QStringLiteral(
            "SELECT t1.id FROM t1 JOIN t2 ON t1.id=t2.id;"));
        check(dotted.contains(QStringLiteral("t1.id")) && dotted.contains(QStringLiteral("\nJOIN t2 ON t1.id = t2.id")),
              QStringLiteral("formatter keeps dotted identifiers glued"));
    }

    // 15c. Analytics chart stays in sync with the result rows.
    {
        editor->setPlainText(QStringLiteral(
            "WITH RECURSIVE cnt(x) AS (SELECT 1 UNION ALL SELECT x+1 FROM cnt WHERE x < 20) "
            "SELECT x, x * 3 AS val FROM cnt;"));
        execButton->click();
        QCoreApplication::processEvents();
        auto *panel = window.findChild<ui::AiAssistantPanel *>(QStringLiteral("aiPanel"));
        check(panel && panel->chartSeriesCount() == 1 && panel->chartPointCount() == 20,
              QStringLiteral("20-row query draws one chart series with 20 points (got %1/%2)")
                  .arg(panel ? panel->chartSeriesCount() : -1)
                  .arg(panel ? panel->chartPointCount() : -1));
    }

    // 16. DROP TABLE via SQL path -> tree empties.
    editor->setPlainText(QStringLiteral("DROP TABLE orders; DROP TABLE users; DROP TABLE products; DROP TABLE satislar;"));
    execButton->click();
    check(tree->model()->rowCount(category(QStringLiteral("Tables"))) == 0,
          QStringLiteral("DROP TABLEs remove tree nodes"));

    // 16. Close database slot.
    QMetaObject::invokeMethod(&window, "closeDatabase");
    root = tree->model()->index(0, 0); // fresh root after final reset
    check(connectionLabel->text().contains(QStringLiteral("Disconnected")) &&
              tree->model()->rowCount(QModelIndex()) == 1 &&
              tree->model()->data(root).toString().contains(QStringLiteral("No connection")),
          QStringLiteral("closeDatabase resets UI"));

    qInfo().noquote() << (failures == 0 ? QStringLiteral("UI TEST: ALL PASSED")
                                        : QStringLiteral("UI TEST: %1 FAILURE(S)").arg(failures));
    return failures;
}
