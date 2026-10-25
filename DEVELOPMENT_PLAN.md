# QuickDb — Development Plan Tracker

A lightweight native SQLite database manager built with **C++17 / Qt 6 Widgets**.
Workspace: `/Users/furkancanisci/db_furcan` · Build dir: `build/`

---

## Milestone 1 — Core Client (v1.0) ✅ COMPLETE

| # | Feature | Status | Where |
|---|---------|--------|-------|
| 1 | Main window: QSplitter layout (sidebar / editor+results) | ✅ | `src/ui/MainWindow.cpp` |
| 2 | Sidebar tree: DB → tables → columns with data types | ✅ | `src/ui/SchemaTreeModel.*` |
| 3 | SQL editor: monospaced, line numbers, current-line highlight | ✅ | `src/ui/QueryEditor.*` |
| 4 | Result grid: QSqlQueryModel subclass, BLOB-safe, NULL greyed | ✅ | `src/ui/SelectQueryModel.*` |
| 5 | Status bar: connection state, messages, execution time | ✅ | `MainWindow::showStatusMessage` |
| 6 | Create new .db file / open existing (QFileDialog) | ✅ | `MainWindow::newDatabase / openDatabase` |
| 7 | Multi-statement execution (quote/comment-aware splitter) | ✅ | `DatabaseManager::runScript` |
| 8 | Ctrl+Enter / Cmd+Enter shortcut + execute selected text | ✅ | `QueryEditor::keyPressEvent` |
| 9 | Clean QSqlError capture → status bar, no crashes | ✅ | `DatabaseManager::runSingle` |
| 10 | DDL UI hooks: Create Table / Add Column / Drop Table dialogs | ✅ | `src/ui/Dialogs/` |
| 11 | Schema Inspector: PRAGMA table_info + sqlite_master parsing | ✅ | `DatabaseManager::tableNames / columnsOf` |
| 12 | Double-click table → `SELECT * ... LIMIT 200` browse | ✅ | `MainWindow::selectTable` |
| 13 | Tree context menu (view rows, gen SELECT, DDL, refresh) | ✅ | `MainWindow::onTreeContextMenu` |

## Milestone 1.1 — Automated Verification & Bug-Hunt (v1.0.1) ✅ COMPLETE

- [x] `smoke_test` — 13/13 core-layer checks (console, no GUI)
- [x] `ui_test` — 22/22 headless UI integration test (`QT_QPA_PLATFORM=offscreen`),
      really clicks every button / Ctrl+Enter / tree slots / dialog widgets
- [x] FIX: schema tree showed tables at top level — DB node was invisible root,
      now tree is properly 3 levels: **DB → tables → columns**
- [x] FIX: result-grid column headers now actually sort (was a no-op with
      `QSqlQueryModel`) → `SelectQueryModel::sort()` re-executes SQL with ORDER BY
- [x] FIX: `removeDatabase: connection still in use` warning → `clearAll()` now
      releases the stored `QSqlDatabase` handle
- [x] FIX: context menu used stale tree selection (Drop/Add Column could target
      the wrong table) → right-click now syncs `currentIndex`
- [x] FIX: `CreateTableDialog` prefilled "id" row now defaults PK ✔ (was unchecked)
- [x] PERF: grid column sizing switched `ResizeToContents` → `Interactive`
      (blocked UI on wide/large result sets); toolbar buttons show text
- [x] Test wiring: widgets expose objectNames (`executeButton`, `queryEditor`,
      `schemaTree`, `resultView`, `connectionLabel`); `connectToPath` is a slot

Run the suites anytime:
```bash
cmake --build build -j8
./build/smoke_test
QT_QPA_PLATFORM=offscreen ./build/ui_test
```

### Build & run
```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
open build/QuickDb.app        # or: ./build/QuickDb.app/Contents/MacOS/QuickDb
```

---

## Milestone 2 — Data Handling (in progress)

- [x] Inline cell editing in result grid (double-click / type; staged until Save)
- [x] Row insert / delete buttons above the grid (+ right-click menu on grid)
- [x] Save / Revert commit flow (`QSqlTableModel::OnManualSubmit` → `submitAll`/`revertAll`)
- [x] Auto mode switch: tree selection → editable table browse; Execute → read-only query grid
- [x] FK introspection in core (`PRAGMA foreign_key_list` → `core::ForeignKeyInfo`)
- [x] **ER Diagram tab** (`ui::ErDiagramView`, QGraphicsScene — table boxes, PK badges, FK arrows with `col → col` labels, wheel-zoom, drag-pan)
- [ ] Pagination or LIMIT-based virtual scroll for large tables
- [ ] Export results: CSV / JSON / SQL inserts (QFileDialog + QStreamWriter)
- [ ] Multiple simultaneous connections (per-DB connection registry, tree shows all)
- [ ] CI hook: run smoke_test + ui_test (offscreen) on every build

### Verification (Milestone 2 so far)

- `ui_test`: **29/29 PASSED** — new checks: browse mode on dbl-click, staged
  add-row + cell edit committed to the DB file, revert discards staged delete,
  saved delete really removes the row, Execute returns grid to read-only mode,
  ER tab reports 2 tables / 1 FK arrow for a `orders.user_id → users.id` FK.
- `smoke_test`: 13/13 PASSED.
- Qt gotcha found: `QSqlTableModel::removeRow()` keeps the row counted in
  `rowCount()` until `submitAll()`/`revertAll()` — tests assert DB-file state
  instead of grid state for staged deletes.

## Milestone 3 — Editor Quality of Life

- [ ] SQL syntax highlighting (QSyntaxHighlighter subclass, keywords/strings/comments)
- [ ] Autocomplete for table & column names (schema metadata already available)
- [ ] Query history panel (persisted via QSettings or a local .quickdb file)
- [ ] Saved queries / snippets
- [ ] Tabbed multiple editor panes

## Milestone 4 — Schema Tooling

- [ ] Edit table (SQLite has no ALTER COLUMN → table-rebuild wizard)
- [ ] Index & view management (create/drop/list)
- [x] Foreign key visualization (FK badges + arrows in ER tab)
- [x] ER diagram (QGraphicsScene, no external deps)

## Milestone 5 — Polish & Distribution

- [ ] App icon + macOS code signing / notarization
- [ ] Dark-mode-aware styling (currently light-tuned)
- [ ] Unit tests for `DatabaseManager` (Qt Test, offscreen)
- [ ] Windows/Linux CI builds (Qt 6 is cross-platform, code has no OS deps)

## Milestone 6 — Offline AI Assistant & Visualizer (v1.1) ✅ COMPLETE

Right-hand dock (`ui::AiAssistantPanel`, 3-column splitter) with two tabs:

| # | Feature | Status | Where |
|---|---------|--------|-------|
| 1 | **Chat tab** — natural-language (TR/EN) → read-only SQL → answer bubbles | ✅ | `src/ui/AiAssistantPanel.*` |
| 2 | **Analytics tab** — QtCharts auto Line/Bar/Pie + TR/EN executive summary | ✅ | `AiAssistantPanel::paintChart` |
| 3 | Schema context snapshot (tables/columns/types + sample values) | ✅ | `src/ai/SchemaContext.*` |
| 4 | Local LLM client (Ollama, `localhost:11434`, **runtime model picker**; default `qwen2.5-coder:3b`) | ✅ | `src/ai/OllamaClient.*`, `AiAssistantPanel` (`aiModel`) |
| 5 | Deterministic offline rule engine (regex intents, no service needed) | ✅ | `src/ai/RuleBasedNlu.*` |
| 6 | Read-only SQL guard + fenced-SQL extraction | ✅ | `RuleBasedNlu::isReadOnlySql / extractSqlFromText` |
| 7 | Chart-type decision + TR/EN manager summary from result stats | ✅ | `src/ai/DataInspector.*` |
| 8 | Flow orchestration: LLM → guard → fallback → execute → insight | ✅ | `src/ai/AiOrchestrator.*` |
| 9 | Status-bar engine badge (Ollama vs Offline rule engine) | ✅ | `MainWindow::updateAiBadge` |
| 10 | Manual-Execute results auto-feed the Analytics chart (no re-query) | ✅ | `AiAssistantPanel::showResultFromModel` |

- [x] `ollama pull qwen2.5:1.5b` (~1 GB) — runs fully on-device. **Note:** the 1.5B
  model is only good at short/medium queries; complex / long SQL needs a stronger
  on-device model. The project now uses **`qwen2.5-coder:3b` (~1.9 GB — the
  strongest code/SQL model that fits a ≤2 GB budget)**, pulled and integrated.
  The app defaults to it, exposes an in-panel model picker, raises the output
  cap (`num_predict 8192`) and generation timeout (180 s), and never silently
  downgrades a failed model answer to a trivial rule query (it shows the real SQL
  + the database error instead).
- [x] View menu → "AI Assistant" toggle (`Ctrl+Shift+A`), default panel width 320 px.

### Verification (Milestone 6)

- `ai_test` — **new offline suite** (no network, no widgets): schema matching, every
  intent → SQL shape (count/sum/trend/topN/lookup + EN), the read-only guard
  allow/reject matrix, fenced-SQL extraction, DataInspector Line/Bar/Pie decisions
  and the TR/EN summaries, plus an end-to-end orchestrator run on a fixture DB.
- `ui_test`: **34/34 PASSED** — new deterministic checks drive the real AI panel
  widgets with the LLM disabled (rule engine only): tabs present, natural-language
  question yields a chartable 6-row trend, the Analytics summary is computed from
  the result, the panel auto-switches to the chart, and a "drop the table" phrasing
  is provably **never** executed as a write.
- `smoke_test`: 13/13 PASSED.

---

## Milestone 7 — QuickDb Pro Feature Wave (v1.2) ✅ COMPLETE

Seven phases delivered in one pass, with **zero regressions** to the prior
baselines (all suites still green and extended — see Verification).

| Phase | Feature | Status | Where |
|-------|---------|--------|-------|
| A | Modern / premium / responsive theme, light↔dark toggle (`Ctrl+D`), collapsible splitters | ✅ | `src/ui/Theme.*`, `MainWindow` View menu |
| B | Multiple simultaneous **result tabs** (Query read-only / Browse editable / AI snapshot) | ✅ | `src/ui/ResultTab.*`, `MainWindow` `m_resultTabs` |
| C | Business-analyst **report** (KPI/trend/distribution/outliers/recommendations, TR+EN), rich chat bubbles, auto **present** SQL→editor + result→grid | ✅ | `src/ai/DataInspector.*`, `AiAssistantPanel`, `MainWindow::onAiPresentResult` |
| D | Driver-agnostic **connection** layer (`ConnectionConfig`, `connect()`, `availableDrivers()`) + Connection Manager dialog with saved profiles (`QSettings`) | ✅ | `src/core/DatabaseManager.*`, `src/ui/Dialogs/ConnectionDialog.*` |
| E | **Import/Export wizards** — CSV / JSON / XML / SQL-dump | ✅ | `src/core/io/{Exporters,Importers}.*`, `src/ui/Dialogs/{Export,Import}Dialog.*` |
| F | Schema objects **Views / Indexes / Triggers** — introspection + DDL + categorized schema tree + per-node context menus | ✅ | `DatabaseManager` (views/indexesOf/triggersOf/create*/drop*), `SchemaTreeModel`, `CreateViewDialog`, `CreateIndexDialog` |
| G | SQL **editor**: syntax highlighting + schema **auto-complete** (`Ctrl+Space`) + **EXPLAIN QUERY PLAN** + **Format SQL** | ✅ | `src/ui/SqlHighlighter.*`, `QueryEditor.*`, `DatabaseManager::explainQueryPlan` |

### Multi-database philosophy
Driver abstraction is now in place (`QSQLITE` wired; `QPSQL`/`QMYSQL`/`QODBC`
become selectable the moment the matching Qt SQL plugin is installed). MongoDB /
Redis / ClickHouse are deliberately **out of scope** — they do not fit the QtSql
row/column model. All AI/network use remains **localhost-only**; the cloud is not
used. XLSX export is out of scope (needs an external library).

### Verification (Milestone 7)
- `smoke_test`: **27/27 PASSED** — adds `EXPLAIN QUERY PLAN` and full
  view/index/trigger create → introspect → drop round-trips.
- `ai_test`: **27/27 PASSED** — adds analyst `reportHtmlTr/En` content checks on
  top of the existing 24.
- `io_test`: **new target, 26/26 PASSED** — CSV/JSON/XML/SQL-dump serialization
  and CSV/JSON parse round-trips, quoted-field handling and type inference.
- `ui_test`: **52/52 PASSED** — multi-tab results, dirty-close confirm, CRUD on
  the active tab, AI auto-present flow, the Connection/Export/Import dialog
  widgets, the categorized schema tree, and the highlighter / auto-completer /
  Format SQL / EXPLAIN editor features.

---

## Architecture Notes

- **`core::DatabaseManager`** — zero Qt-Widgets dependency; owns connection
  lifecycle (unique per-open connection name, RAII removal on close), script
  splitting, execution with timing, schema introspection, DDL builders with
  identifier quoting (`"..."`, inner `"` escaped).
- **`ui::` layer** — MainWindow wires widgets to DatabaseManager only;
  models (`SchemaTreeModel`, `SelectQueryModel`) are proper
  QAbstractItemModel/QSqlQueryModel subclasses, no logic in views.
- **Constraints honored** — the AI feature is **fully offline**: it talks only to a
  local Ollama service over `localhost` (loopback — no query or row ever leaves the
  machine) and, when that service is absent, falls back to a deterministic on-device
  rule engine that needs no service at all. **No cloud/external LLM, no outbound
  network.** The AI produces **read-only SQL only** (enforced by a guard).

## Known Limitations (v1.0)

- Single active connection at a time (Milestone 2 lifts this).
- Result grid is read-only for ad-hoc SQL; editing works only in table-browse
  mode (tree selection / double-click), never for arbitrary query results.
- ER layout is a simple auto-grid (no drag-to-arrange / saved positions yet).
- `ALTER TABLE ADD COLUMN` only (SQLite restriction; column drop needs rebuild wizard).
