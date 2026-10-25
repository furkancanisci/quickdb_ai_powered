#include "core/DatabaseManager.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlField>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>

#include <memory>

namespace core {

namespace {

int g_connectionCounter = 0;

// Non-SQLite drivers introspect the `public` schema (QuickDb's PG convention).
const char *kPgSchema = "public";

bool startsResultSet(const QString &trimmedSql)
{
    static const QStringList prefixes = {
        QStringLiteral("select"), QStringLiteral("with"),
        QStringLiteral("pragma"), QStringLiteral("explain"),
        QStringLiteral("values"), QStringLiteral("table")
    };
    const QString lower = trimmedSql.toLower();
    for (const QString &p : prefixes) {
        if (lower.startsWith(p))
            return true;
    }
    return false;
}

} // namespace

/// Splits a SQL script into individual statements. Quote-aware
/// ('single', "double", `backtick`) and comment-aware (-- and /* */).
static QStringList splitStatements(const QString &sql)
{
    QStringList statements;
    QString current;
    enum class State { Normal, SingleQuote, DoubleQuote, Backtick, LineComment, BlockComment };
    State state = State::Normal;

    for (int i = 0; i < sql.size(); ++i) {
        const QChar c = sql.at(i);
        const QChar next = (i + 1 < sql.size()) ? sql.at(i + 1) : QChar();

        switch (state) {
        case State::Normal:
            if (c == QLatin1Char('\'')) { state = State::SingleQuote; current += c; }
            else if (c == QLatin1Char('"')) { state = State::DoubleQuote; current += c; }
            else if (c == QLatin1Char('`')) { state = State::Backtick; current += c; }
            else if (c == '-' && next == QLatin1Char('-')) { state = State::LineComment; current += c; }
            else if (c == '/' && next == QLatin1Char('*')) { state = State::BlockComment; current += c; }
            else if (c == QLatin1Char(';')) {
                const QString trimmed = current.trimmed();
                if (!trimmed.isEmpty())
                    statements.append(trimmed);
                current.clear();
            } else {
                current += c;
            }
            break;
        case State::SingleQuote:
            current += c;
            if (c == QLatin1Char('\'')) {
                if (next == QLatin1Char('\'')) { current += next; ++i; } // escaped ''
                else state = State::Normal;
            }
            break;
        case State::DoubleQuote:
            current += c;
            if (c == QLatin1Char('"')) state = State::Normal;
            break;
        case State::Backtick:
            current += c;
            if (c == QLatin1Char('`')) state = State::Normal;
            break;
        case State::LineComment:
            current += c;
            if (c == QLatin1Char('\n')) state = State::Normal;
            break;
        case State::BlockComment:
            current += c;
            if (c == QLatin1Char('*') && next == QLatin1Char('/')) {
                current += next; ++i;
                state = State::Normal;
            }
            break;
        }
    }
    const QString trimmed = current.trimmed();
    if (!trimmed.isEmpty())
        statements.append(trimmed);
    return statements;
}

DatabaseManager::DatabaseManager() = default;

DatabaseManager::~DatabaseManager()
{
    closeDatabase();
}

QString DatabaseManager::quoteIdentifier(const QString &id)
{
    QString escaped = id;
    escaped.replace(QLatin1Char('"'), QLatin1String("\"\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

bool DatabaseManager::createDatabase(const QString &filePath, QString *error)
{
    // sqlite3 creates the file on connect; force schema init so the file
    // is a valid (empty) database immediately.
    if (!openDatabase(filePath, error))
        return false;

    QSqlQuery pragma(m_db);
    if (!pragma.exec(QStringLiteral("PRAGMA user_version = 1;"))) {
        if (error)
            *error = pragma.lastError().text();
        return false;
    }
    m_lastAffectedRows = 0;
    return true;
}

QStringList DatabaseManager::availableDrivers()
{
    return QSqlDatabase::drivers();
}

bool DatabaseManager::connect(const ConnectionConfig &cfg, QString *error)
{
    closeDatabase();

    const QString driver = cfg.driverType.isEmpty()
                               ? QStringLiteral("QSQLITE") : cfg.driverType;
    if (!QSqlDatabase::isDriverAvailable(driver)) {
        if (error)
            *error = QStringLiteral("Driver '%1' not available in this build.").arg(driver);
        return false;
    }

    m_connectionName = QStringLiteral("quickdb_conn_%1").arg(++g_connectionCounter);
    m_db = QSqlDatabase::addDatabase(driver, m_connectionName);

    if (cfg.isSqlite()) {
        m_db.setDatabaseName(cfg.filePath);
        QString opts = cfg.connOptions;
        if (!opts.contains(QStringLiteral("QSQLITE_BUSY_TIMEOUT")))
            opts += (opts.isEmpty() ? QString() : QStringLiteral(";"))
                    + QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000");
        m_db.setConnectOptions(opts);
    } else {
        // Server-style drivers (QPSQL / QMYSQL / QODBC).
        m_db.setHostName(cfg.host);
        if (cfg.port > 0)
            m_db.setPort(cfg.port);
        m_db.setDatabaseName(cfg.databaseName);
        if (!cfg.userName.isEmpty())
            m_db.setUserName(cfg.userName);
        if (!cfg.password.isEmpty())
            m_db.setPassword(cfg.password);
        if (!cfg.connOptions.isEmpty())
            m_db.setConnectOptions(cfg.connOptions);
    }

    if (!m_db.open()) {
        if (error)
            *error = m_db.lastError().text();
        QSqlDatabase::removeDatabase(m_connectionName);
        m_db = QSqlDatabase();
        return false;
    }

    m_config = cfg;
    m_filePath = cfg.isSqlite() ? cfg.filePath : cfg.databaseName;
    return true;
}

bool DatabaseManager::openDatabase(const QString &filePath, QString *error)
{
    ConnectionConfig cfg;
    cfg.driverType = QStringLiteral("QSQLITE");
    cfg.filePath = filePath;
    cfg.title = QFileInfo(filePath).fileName();
    return connect(cfg, error);
}

void DatabaseManager::closeDatabase()
{
    if (m_db.isValid()) {
        m_db.close();
        const QString name = m_connectionName;
        m_db = QSqlDatabase();          // drop handle before removing
        QSqlDatabase::removeDatabase(name);
    }
    m_filePath.clear();
    m_config = ConnectionConfig();
}

QString DatabaseManager::currentDatabaseName() const
{
    const QFileInfo info(m_filePath);
    return info.fileName();
}

QueryResult DatabaseManager::runScript(const QString &sql)
{
    QueryResult result;
    const QStringList statements = splitStatements(sql);
    if (statements.isEmpty()) {
        result.success = false;
        result.errorMessage = QStringLiteral("Nothing to execute (empty statement).");
        return result;
    }

    QElapsedTimer timer;
    timer.start();

    for (int i = 0; i < statements.size(); ++i) {
        QueryResult single = runSingle(statements.at(i));
        result.affectedRows += single.affectedRows;
        if (!single.success) {
            result.failedAtStatement = i;
            result.errorMessage = QStringLiteral("Statement %1: %2")
                                      .arg(i + 1)
                                      .arg(single.errorMessage);
            result.elapsedMs = timer.elapsed();
            return result;
        }
        if (single.hasResultSet) {
            result.hasResultSet = true;
            result.query = std::move(single.query);
        }
    }

    result.success = true;
    result.elapsedMs = timer.elapsed();
    return result;
}

QueryResult DatabaseManager::runSingle(const QString &sql)
{
    QueryResult result;
    if (!m_db.isOpen()) {
        result.errorMessage = QStringLiteral("No active database connection.");
        return result;
    }

    QElapsedTimer timer;
    timer.start();

    QSqlQuery query(m_db);
    query.setForwardOnly(false);
    const bool executed = query.exec(sql);
    const QSqlError error = query.lastError();

    result.elapsedMs = timer.elapsed();
    if (!executed || error.isValid()) {
        result.errorMessage = error.text().isEmpty()
                                  ? QStringLiteral("Execution failed.")
                                  : error.text();
        return result;
    }

    result.success = true;
    result.hasResultSet = query.isSelect() || startsResultSet(sql);
    result.affectedRows = query.isSelect() ? 0 : query.numRowsAffected();
    if (result.hasResultSet)
        result.query = std::move(query);
    return result;
}

QueryResult DatabaseManager::explainQueryPlan(const QString &sql)
{
    QString body = sql.trimmed();
    while (body.endsWith(QLatin1Char(';')))
        body.chop(1);
    if (body.isEmpty()) {
        QueryResult r;
        r.errorMessage = QStringLiteral("Nothing to explain.");
        return r;
    }
    const QString lower = body.left(10).toLower();
    if (lower.startsWith(QStringLiteral("explain")))
        return runSingle(body);
    if (m_config.isPostgres()) {
        // PostgreSQL's default EXPLAIN yields a single 'QUERY PLAN' text
        // column (already a result set), so no JSON unwrapping is needed.
        return runSingle(QStringLiteral("EXPLAIN %1").arg(body));
    }
    return runSingle(QStringLiteral("EXPLAIN QUERY PLAN %1").arg(body));
}

QStringList DatabaseManager::tableNames() const
{
    QStringList names;
    if (!m_db.isOpen())
        return names;

    if (m_config.isSqlite()) {
        QSqlQuery query(m_db);
        if (query.exec(QStringLiteral(
                "SELECT name FROM sqlite_master "
                "WHERE type IN ('table','view') AND name NOT LIKE 'sqlite_%' "
                "ORDER BY type DESC, name COLLATE NOCASE;"))) {
            while (query.next())
                names << query.value(0).toString();
        }
        return names;
    }

    // Generic driver fallback (PostgreSQL / MySQL / ODBC).
    if (m_config.isPostgres()) {
        QSqlQuery query(m_db);
        if (query.exec(QStringLiteral(
                "SELECT tablename FROM pg_tables WHERE schemaname = '%1' "
                "UNION ALL "
                "SELECT viewname FROM pg_views WHERE schemaname = '%1' "
                "ORDER BY 1;").arg(QLatin1String(kPgSchema)))) {
            while (query.next())
                names << query.value(0).toString();
            return names;
        }
    }
    return m_db.tables();
}

QVector<ColumnInfo> DatabaseManager::columnsOf(const QString &table) const
{
    QVector<ColumnInfo> columns;
    if (!m_db.isOpen() || table.isEmpty())
        return columns;

    if (m_config.isSqlite()) {
        QSqlQuery query(m_db);
        query.prepare(QStringLiteral("PRAGMA table_info(%1);").arg(quoteIdentifier(table)));
        if (!query.exec())
            return columns;

        while (query.next()) {
            ColumnInfo info;
            info.name = query.value(1).toString();
            info.type = query.value(2).toString().toUpper();
            info.notNull = query.value(3).toInt() != 0;
            info.defaultValue = query.value(4).toString();
            info.primaryKey = query.value(5).toInt() != 0;
            columns.append(info);
        }
        return columns;
    }

    // PostgreSQL: information_schema for the layout + PK membership.
    if (m_config.isPostgres()) {
        QSqlQuery query(m_db);
        query.prepare(QStringLiteral(
            "SELECT c.column_name, c.data_type, "
            "  (c.is_nullable = 'NO'), COALESCE(c.column_default, ''), "
            "  (pk.column_name IS NOT NULL) "
            "FROM information_schema.columns c "
            "LEFT JOIN ( "
            "  SELECT kcu.table_name, kcu.column_name "
            "  FROM information_schema.table_constraints tc "
            "  JOIN information_schema.key_column_usage kcu "
            "    ON tc.constraint_name = kcu.constraint_name "
            "   AND tc.table_schema = kcu.table_schema "
            "  WHERE tc.constraint_type = 'PRIMARY KEY' "
            ") pk ON pk.table_name = c.table_name AND pk.column_name = c.column_name "
            "WHERE c.table_schema = '%1' AND c.table_name = :tbl "
            "ORDER BY c.ordinal_position;").arg(QLatin1String(kPgSchema)));
        query.bindValue(QStringLiteral(":tbl"), table);
        if (!query.exec())
            return columns;
        while (query.next()) {
            ColumnInfo info;
            info.name = query.value(0).toString();
            info.type = query.value(1).toString().toUpper();
            info.notNull = query.value(2).toBool();
            info.defaultValue = query.value(3).toString();
            info.primaryKey = query.value(4).toBool();
            columns.append(info);
        }
        return columns;
    }

    // Generic driver fallback: read the table's record layout.
    const QSqlRecord rec = m_db.record(table);
    for (int i = 0; i < rec.count(); ++i) {
        const QSqlField f = rec.field(i);
        ColumnInfo info;
        info.name = f.name();
        info.type = QString::fromUtf8(f.metaType().name()).toUpper();
        info.notNull = false;
        info.defaultValue = f.defaultValue().toString();
        info.primaryKey = rec.isGenerated(i);
        columns.append(info);
    }
    return columns;
}

QVector<ForeignKeyInfo> DatabaseManager::foreignKeysOf(const QString &table) const
{
    QVector<ForeignKeyInfo> fks;
    if (!m_db.isOpen() || table.isEmpty())
        return fks;

    if (m_config.isPostgres()) {
        QSqlQuery query(m_db);
        query.prepare(QStringLiteral(
            "SELECT kcu.column_name, ccu.table_name, ccu.column_name "
            "FROM information_schema.table_constraints tc "
            "JOIN information_schema.key_column_usage kcu "
            "  ON tc.constraint_name = kcu.constraint_name "
            " AND tc.table_schema = kcu.table_schema "
            "JOIN information_schema.constraint_column_usage ccu "
            "  ON ccu.constraint_name = tc.constraint_name "
            " AND ccu.table_schema = tc.table_schema "
            "WHERE tc.constraint_type = 'FOREIGN KEY' "
            "  AND tc.table_schema = '%1' AND tc.table_name = :tbl;")
                        .arg(QLatin1String(kPgSchema)));
        query.bindValue(QStringLiteral(":tbl"), table);
        if (query.exec()) {
            while (query.next()) {
                ForeignKeyInfo fk;
                fk.fromColumn = query.value(0).toString();
                fk.toTable = query.value(1).toString();
                fk.toColumn = query.value(2).toString();
                if (!fk.toTable.isEmpty() && !fk.fromColumn.isEmpty())
                    fks.append(fk);
            }
        }
        return fks;
    }

    // PRAGMA foreign_key_list columns: id, seq, table, from, to, on_update, on_delete, match
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("PRAGMA foreign_key_list(%1);").arg(quoteIdentifier(table)));
    if (!query.exec())
        return fks;

    while (query.next()) {
        ForeignKeyInfo fk;
        fk.toTable = query.value(2).toString();
        fk.fromColumn = query.value(3).toString();
        fk.toColumn = query.value(4).toString();
        if (!fk.toTable.isEmpty() && !fk.fromColumn.isEmpty())
            fks.append(fk);
    }
    return fks;
}

bool DatabaseManager::createTable(const TableDefinition &definition, QString *error)
{
    if (definition.tableName.trimmed().isEmpty() || definition.columns.isEmpty()) {
        if (error)
            *error = QStringLiteral("Table name and at least one column are required.");
        return false;
    }

    QStringList columnParts;
    QStringList pkColumns;
    for (const ColumnSpec &col : definition.columns) {
        if (col.name.trimmed().isEmpty())
            continue;
        QString part = quoteIdentifier(col.name) + QLatin1Char(' ') + col.type;
        if (col.primaryKey) {
            part += QStringLiteral(" PRIMARY KEY");
            if (m_config.isSqlite()
                && col.type.compare(QLatin1String("INTEGER"), Qt::CaseInsensitive) == 0)
                part += QStringLiteral(" AUTOINCREMENT");
        }
        if (col.notNull && !col.primaryKey)
            part += QStringLiteral(" NOT NULL");
        if (!col.defaultValue.trimmed().isEmpty())
            part += QStringLiteral(" DEFAULT %1").arg(col.defaultValue.trimmed());
        columnParts << part;
        if (col.primaryKey)
            pkColumns << quoteIdentifier(col.name);
    }

    // Composite primary key: strip inline PKs and add a table constraint.
    if (pkColumns.size() > 1) {
        columnParts.clear();
        for (const ColumnSpec &col : definition.columns) {
            if (col.name.trimmed().isEmpty())
                continue;
            QString part = quoteIdentifier(col.name) + QLatin1Char(' ') + col.type;
            if (col.notNull)
                part += QStringLiteral(" NOT NULL");
            if (!col.defaultValue.trimmed().isEmpty())
                part += QStringLiteral(" DEFAULT %1").arg(col.defaultValue.trimmed());
            columnParts << part;
        }
        columnParts << QStringLiteral("PRIMARY KEY (%1)").arg(pkColumns.join(QLatin1Char(',')));
    }

    const QString ddl = QStringLiteral("CREATE TABLE %1 (%2);")
                            .arg(quoteIdentifier(definition.tableName),
                                 columnParts.join(QLatin1Char(',')));

    QueryResult result = runSingle(ddl);
    if (!result.success && error)
        *error = result.errorMessage;
    return result.success;
}

bool DatabaseManager::addColumn(const QString &table, const ColumnSpec &column, QString *error)
{
    if (column.name.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("Column name is required.");
        return false;
    }

    QString part = quoteIdentifier(column.name) + QLatin1Char(' ') + column.type;
    if (column.notNull)
        part += QStringLiteral(" NOT NULL");
    const QString defaultValue = column.defaultValue.trimmed();
    if (!defaultValue.isEmpty())
        part += QStringLiteral(" DEFAULT %1").arg(defaultValue);

    const QString ddl = QStringLiteral("ALTER TABLE %1 ADD COLUMN %2;")
                            .arg(quoteIdentifier(table), part);

    QueryResult result = runSingle(ddl);
    if (!result.success && error)
        *error = result.errorMessage;
    return result.success;
}

bool DatabaseManager::dropTable(const QString &table, QString *error)
{
    const QString ddl = QStringLiteral("DROP TABLE IF EXISTS %1;").arg(quoteIdentifier(table));
    QueryResult result = runSingle(ddl);
    if (!result.success && error)
        *error = result.errorMessage;
    return result.success;
}

// ---------------------------------------------------------------------------
// Views / Indexes / Triggers
// ---------------------------------------------------------------------------

QVector<ViewInfo> DatabaseManager::views() const
{
    QVector<ViewInfo> out;
    if (!m_db.isOpen())
        return out;
    if (m_config.isPostgres()) {
        QSqlQuery query(m_db);
        if (query.exec(QStringLiteral(
                "SELECT table_name, view_definition FROM information_schema.views "
                "WHERE table_schema = '%1' ORDER BY table_name;")
                    .arg(QLatin1String(kPgSchema)))) {
            while (query.next()) {
                ViewInfo v;
                v.name = query.value(0).toString();
                v.sql = query.value(1).toString();
                out.append(v);
            }
        }
        return out;
    }
    if (!m_config.isSqlite())
        return out;
    QSqlQuery query(m_db);
    if (query.exec(QStringLiteral(
            "SELECT name, sql FROM sqlite_master "
            "WHERE type = 'view' ORDER BY name COLLATE NOCASE;"))) {
        while (query.next()) {
            ViewInfo v;
            v.name = query.value(0).toString();
            v.sql = query.value(1).toString();
            out.append(v);
        }
    }
    return out;
}

QVector<IndexInfo> DatabaseManager::indexesOf(const QString &table) const
{
    QVector<IndexInfo> out;
    if (!m_db.isOpen() || table.isEmpty())
        return out;

    if (m_config.isPostgres()) {
        QSqlQuery query(m_db);
        query.prepare(QStringLiteral(
            "SELECT i.relname, idx.indisunique, idx.indisprimary, "
            "  string_agg(a.attname, ',' ORDER BY s.ord) "
            "FROM pg_index idx "
            "JOIN pg_class t ON t.oid = idx.indrelid "
            "JOIN pg_class i ON i.oid = idx.indexrelid "
            "JOIN pg_namespace n ON n.oid = t.relnamespace "
            "JOIN LATERAL unnest(idx.indkey) WITH ORDINALITY AS s(attnum, ord) ON true "
            "JOIN pg_attribute a ON a.attrelid = t.oid AND a.attnum = s.attnum "
            "WHERE n.nspname = '%1' AND t.relname = :tbl "
            "GROUP BY i.relname, idx.indisunique, idx.indisprimary "
            "ORDER BY i.relname;").arg(QLatin1String(kPgSchema)));
        query.bindValue(QStringLiteral(":tbl"), table);
        if (query.exec()) {
            while (query.next()) {
                IndexInfo info;
                info.name = query.value(0).toString();
                info.unique = query.value(1).toBool();
                info.fromConstraint = query.value(2).toBool();
                info.columns = query.value(3).toString().split(QLatin1Char(','), Qt::SkipEmptyParts);
                info.tableName = table;
                out.append(info);
            }
        }
        return out;
    }

    if (!m_config.isSqlite())
        return out;

    // PRAGMA index_list: seq, name, unique, origin, partial
    QSqlQuery list(m_db);
    list.prepare(QStringLiteral("PRAGMA index_list(%1);").arg(quoteIdentifier(table)));
    if (!list.exec())
        return out;
    while (list.next()) {
        IndexInfo info;
        info.name = list.value(1).toString();
        info.unique = list.value(2).toInt() != 0;
        const QString origin = list.value(3).toString();
        info.fromConstraint = (origin == QStringLiteral("u") || origin == QStringLiteral("pk"))
                              || info.name.startsWith(QStringLiteral("sqlite_autoindex"));
        info.tableName = table;
        // PRAGMA index_info: seqno, cid, name
        QSqlQuery cols(m_db);
        cols.prepare(QStringLiteral("PRAGMA index_info(%1);").arg(quoteIdentifier(info.name)));
        if (cols.exec()) {
            while (cols.next())
                info.columns << cols.value(2).toString();
        }
        out.append(info);
    }
    return out;
}

QVector<TriggerInfo> DatabaseManager::triggersOf(const QString &table) const
{
    QVector<TriggerInfo> out;
    if (!m_db.isOpen() || table.isEmpty())
        return out;

    if (m_config.isPostgres()) {
        QSqlQuery query(m_db);
        query.prepare(QStringLiteral(
            "SELECT tg.tgname, pg_get_triggerdef(tg.oid) "
            "FROM pg_trigger tg "
            "JOIN pg_class t ON t.oid = tg.tgrelid "
            "JOIN pg_namespace n ON n.oid = t.relnamespace "
            "WHERE n.nspname = '%1' AND t.relname = :tbl AND NOT tg.tgisinternal "
            "ORDER BY tg.tgname;").arg(QLatin1String(kPgSchema)));
        query.bindValue(QStringLiteral(":tbl"), table);
        if (query.exec()) {
            while (query.next()) {
                TriggerInfo t;
                t.name = query.value(0).toString();
                t.tableName = table;
                t.sql = query.value(1).toString();
                out.append(t);
            }
        }
        return out;
    }

    if (!m_config.isSqlite())
        return out;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT name, sql FROM sqlite_master WHERE type = 'trigger' AND tbl_name = ?1 "
        "ORDER BY name COLLATE NOCASE;"));
    query.addBindValue(table);
    if (query.exec()) {
        while (query.next()) {
            TriggerInfo t;
            t.name = query.value(0).toString();
            t.tableName = table;
            t.sql = query.value(1).toString();
            out.append(t);
        }
    }
    return out;
}

QString DatabaseManager::sqlFor(const QString &objectName) const
{
    if (!m_db.isOpen() || objectName.isEmpty())
        return QString();

    if (m_config.isPostgres()) {
        // Views, then indexes, then triggers - whichever owns the name.
        QSqlQuery view(m_db);
        view.prepare(QStringLiteral(
            "SELECT pg_get_viewdef(c.oid, true) FROM pg_class c "
            "JOIN pg_namespace n ON n.oid = c.relnamespace "
            "WHERE n.nspname = '%1' AND c.relname = :o AND c.relkind = 'v';")
                         .arg(QLatin1String(kPgSchema)));
        view.bindValue(QStringLiteral(":o"), objectName);
        if (view.exec() && view.next() && !view.value(0).toString().trimmed().isEmpty())
            return QStringLiteral("CREATE VIEW %1 AS %2")
                .arg(quoteIdentifier(objectName), view.value(0).toString().trimmed());

        QSqlQuery idx(m_db);
        idx.prepare(QStringLiteral(
            "SELECT indexdef FROM pg_indexes WHERE schemaname = '%1' AND indexname = :o;")
                     .arg(QLatin1String(kPgSchema)));
        idx.bindValue(QStringLiteral(":o"), objectName);
        if (idx.exec() && idx.next())
            return idx.value(0).toString();

        QSqlQuery trg(m_db);
        trg.prepare(QStringLiteral("SELECT pg_get_triggerdef(oid) FROM pg_trigger WHERE tgname = :o;"));
        trg.bindValue(QStringLiteral(":o"), objectName);
        if (trg.exec() && trg.next())
            return trg.value(0).toString();
        return QString();
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT sql FROM sqlite_master WHERE name = ?1;"));
    query.addBindValue(objectName);
    if (query.exec() && query.next())
        return query.value(0).toString();
    return QString();
}

bool DatabaseManager::createView(const QString &name, const QString &selectSql, QString *error)
{
    QString body = selectSql.trimmed();
    while (body.endsWith(QLatin1Char(';')))
        body.chop(1);
    const QString ddl = QStringLiteral("CREATE VIEW %1 AS %2;").arg(quoteIdentifier(name), body);
    QueryResult result = runSingle(ddl);
    if (!result.success && error)
        *error = result.errorMessage;
    return result.success;
}

bool DatabaseManager::dropView(const QString &name, QString *error)
{
    const QString ddl = QStringLiteral("DROP VIEW IF EXISTS %1;").arg(quoteIdentifier(name));
    QueryResult result = runSingle(ddl);
    if (!result.success && error)
        *error = result.errorMessage;
    return result.success;
}

bool DatabaseManager::createIndex(const QString &name, const QString &table,
                                  const QStringList &columns, bool unique, QString *error)
{
    QStringList quoted;
    for (const QString &c : columns)
        quoted << quoteIdentifier(c);
    QString idxName = name.trimmed();
    if (idxName.isEmpty())
        idxName = QStringLiteral("idx_%1_%2").arg(table, columns.join(QStringLiteral("_")));
    const QString ddl = QStringLiteral("CREATE %1INDEX IF NOT EXISTS %2 ON %3 (%4);")
        .arg(unique ? QStringLiteral("UNIQUE ") : QString(),
             quoteIdentifier(idxName), quoteIdentifier(table), quoted.join(QStringLiteral(", ")));
    QueryResult result = runSingle(ddl);
    if (!result.success && error)
        *error = result.errorMessage;
    return result.success;
}

bool DatabaseManager::dropIndex(const QString &name, QString *error)
{
    const QString ddl = QStringLiteral("DROP INDEX IF EXISTS %1;").arg(quoteIdentifier(name));
    QueryResult result = runSingle(ddl);
    if (!result.success && error)
        *error = result.errorMessage;
    return result.success;
}

bool DatabaseManager::createTrigger(const QString &ddl, QString *error)
{
    QueryResult result = runSingle(ddl);
    if (!result.success && error)
        *error = result.errorMessage;
    return result.success;
}

bool DatabaseManager::dropTrigger(const QString &name, QString *error)
{
    const QString ddl = QStringLiteral("DROP TRIGGER IF EXISTS %1;").arg(quoteIdentifier(name));
    QueryResult result = runSingle(ddl);
    if (!result.success && error)
        *error = result.errorMessage;
    return result.success;
}

} // namespace core
