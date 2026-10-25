# QuickDb Pro — SQL & AI Analytics

> **Language:** English · [Türkçe →](./README.md)

**QuickDb Pro** is a desktop database-analysis app that runs on SQLite (and
PostgreSQL/MySQL/ODBC when the matching Qt driver plugin is installed) and
**generates SQL from natural language**. Built with Qt 6 / C++17, it combines a
classic SQL client (schema tree, query editor, editable grid, ER diagram,
import/export) with a local **AI assistant + business analyst**. Data stays on
your machine — the only LLM used is a local [Ollama](https://ollama.com) server
(`http://localhost:11434`).

---

## ✨ Features

### Database & SQL
- **Schema tree:** Tables / Views / Indexes / Triggers categories with columns, types and foreign keys.
- **Query editor:** syntax highlighting, line numbers, schema auto-completion (`Ctrl+Space`), run-selected-statement, `Ctrl/Cmd+Enter` to execute, basic formatting and `EXPLAIN QUERY PLAN`.
- **Multiple result tabs:** read-only query results and **editable** table browsing (CRUD: add/delete row, ✔ Save / ↺ Revert — changes are staged until saved).
- **ER diagram:** tables and foreign-key relations drawn automatically, with zoom/pan.
- **DDL dialogs:** create table, add column, create view/index.
- **Import / Export:** CSV, JSON, XML, SQL dump (with live preview).
- **Driver-agnostic connections:** SQLite works out of the box; QPSQL/QMYSQL/QODBC ready once the plugin is installed.
- **Theme:** Light / Dark mode (`Ctrl+D`).

### AI Assistant (dual-model)
- **Query-generation model:** natural language → a single read-only SQL statement (default `qwen2.5-coder:3b`).
- **Business-analyst model:** turns result rows into prose commentary (default `qwen2.5:1.5b`).
- **Offline rule engine (fallback):** when Ollama is off, TR/EN intent matching (count/sum/trend/topN/lookup/select) still works.
- **Analytics tab:** automatic axis selection + line/bar/pie chart and a deterministic executive report (KPIs, trend, distribution, concentration, outliers).
- **Safety guard:** only `SELECT/WITH/PRAGMA/EXPLAIN` are accepted; write operations are rejected.
- **Intent recognition:** casual messages like "hello" are answered as chat without generating SQL.
- **Anti-hallucination:** the analyst only uses the provided columns/values and stays silent when there is no meaningful numeric data to interpret.

### Web Dashboard (localhost)
- **Pin any result to the panel:** each pinned result becomes a new sidebar page
  with an auto-generated title (e.g. the query's main table → "Orders").
- Pages render a **data table + automatic bar chart**; the whole UI is
  server-side HTML — no external assets or JavaScript, fully offline.
- One click to open: the green **Dashboard / Web Panel** button starts the server
  and opens `http://127.0.0.1:<port>/` in your browser.

---

## 🧱 Architecture

Layered design — widgets live only in `ui/`, so the business logic is testable
independently of the GUI:

```
┌─────────────────────────────────────────────┐
│  ui/    Qt Widgets — view & interaction      │  the only widget layer
├─────────────────────────────────────────────┤
│  ai/    offline reasoning (widget-free)      │  LLM/rules → SQL → insight
├─────────────────────────────────────────────┤
│  core/  DatabaseManager + io + web (DB logic) │  zero widget dependency
├─────────────────────────────────────────────┤
│  Qt6::Sql → QSQLITE/QPSQL/... → database      │
└─────────────────────────────────────────────┘
```

| Directory | Contents |
|---|---|
| `src/core/` | `DatabaseManager` (connections, queries, schema introspection, DDL), `io/` (Exporters/Importers), `web/DashboardServer` (localhost HTTP panel) |
| `src/ai/` | `SchemaContext`, `OllamaClient`, `RuleBasedNlu`, `DataInspector`, `AiOrchestrator` |
| `src/ui/` | `MainWindow`, `AiAssistantPanel`, `QueryEditor`, `ResultTab`, `SchemaTreeModel`, `ErDiagramView`, `Theme`, `Dialogs/` |
| `test/` | `smoke`, `io`, `ai`, `ui` (headless) + live `e2e_*` drivers |

For a file-by-file breakdown see [TECHNICAL_GUIDE.md](./TECHNICAL_GUIDE.md) (Turkish) · roadmap: [DEVELOPMENT_PLAN.md](./DEVELOPMENT_PLAN.md)

---

## 🔧 Requirements

- **CMake** ≥ 3.21
- A **C++17** compiler (Clang/Apple Clang or GCC)
- **Qt 6** (tested with 6.9+) — components: `Widgets Sql Charts Network Xml Test`
- *(optional, for AI)* **[Ollama](https://ollama.com)** with a working model:
  ```bash
  ollama pull qwen2.5-coder:3b   # query generation
  ollama pull qwen2.5:1.5b       # business-analyst commentary
  ```
- *(optional, for PostgreSQL/MySQL)* the matching Qt SQL driver plugin.

Install Qt on macOS (Homebrew):
```bash
brew install qt
```

---

## 🚀 Build & Run

```bash
# Configure (adjust the Qt path if needed)
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt

# Build all targets
cmake --build build -j4

# Run the app
./build/QuickDb.app/Contents/MacOS/QuickDb      # macOS
# or:  open build/QuickDb.app
```

On Linux/Windows just point `CMAKE_PREFIX_PATH` at your Qt installation
(e.g. `-DCMAKE_PREFIX_PATH=~/Qt/6.9.0/clang_64`).

---

## ✅ Tests

```bash
cmake --build build -j4

./build/smoke_test                 # core/DB layer (no GUI)
./build/io_test                    # import/export round-trip
./build/ai_test                    # offline AI logic (rules + analyst)
QT_QPA_PLATFORM=offscreen ./build/ui_test   # headless GUI integration test
```

**Live drivers (require network/Ollama)** — run manually:
```bash
./build/e2e_ai_test samples/company_analytics.db   # end-to-end AI pipeline
./build/e2e_replay  <question-file>                # replay a single question
QT_PLUGIN_PATH=build ./build/pg_e2e_test          # PostgreSQL (if set up)
```

---

## 📦 Sample Data

A ready-to-use analytics database ships under `samples/`:
- `samples/company_analytics.db` — orders, products, categories, customers
- `samples/company_analytics.sql` — the setup script for the same database

After launching the app, choose **File ▸ Open** to pick the `.db` file and try the
sample queries / AI questions.

---

## 🖥️ Using the Web Dashboard

1. Run a query or ask the AI assistant (the result lands in a Results tab).
2. Tick **"📌 Pin to Dashboard"** (Turkish UI: "Panoya Ekle") in the CRUD row — the
   result becomes a new page with an auto-generated title.
3. Click **"🟢 Dashboard / Web Panel"**; your browser opens
   `http://127.0.0.1:<port>/`. Navigate pages from the sidebar. The server shuts
   down together with the app.

---

## ⚙️ Configuration Notes

- **With AI disabled** the app automatically falls back to the deterministic rule
  engine; the full SQL/analysis flow works offline.
- **The Ollama endpoint** is fixed at `http://localhost:11434` — data never leaves
  the device.
- **The dashboard port** prefers `8765`; if busy it falls back to a free ephemeral port.
- Saved connection profiles and preferences are stored under `QSettings` (`QuickDb`).

---

## 📁 Project Layout

```
db_furcan/
├── src/{core,ai,ui}/        # application sources
├── test/                    # tests (smoke/io/ai/ui + e2e)
├── samples/                 # sample database + script
├── CMakeLists.txt           # build definitions
├── TECHNICAL_GUIDE.md       # file-by-file technical guide (TR)
├── DEVELOPMENT_PLAN.md      # roadmap
├── README.md                # Turkish version
└── README.en.md             # this file (English)
```

---

## 📄 License

This project is a sample application for development/learning purposes. An open
source license (e.g. MIT) can be added if desired.
