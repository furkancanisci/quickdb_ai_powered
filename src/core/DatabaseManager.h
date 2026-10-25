#pragma once

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QStringList>
#include <QVector>

namespace core {

/// Metadata describing a single column of a SQLite table.
struct ColumnInfo {
    QString name;
    QString type;
    bool notNull = false;
    bool primaryKey = false;
    QString defaultValue;
};

/// One foreign-key relation parsed from PRAGMA foreign_key_list.
struct ForeignKeyInfo {
    QString fromColumn;
    QString toTable;
    QString toColumn;
};

/// Column specification used when building DDL statements.
struct ColumnSpec {
    QString name;
    QString type = QStringLiteral("TEXT");
    bool notNull = false;
    bool primaryKey = false;
    QString defaultValue;
};

/// Definition of a table used by the Create Table dialog.
struct TableDefinition {
    QString tableName;
    QVector<ColumnSpec> columns;
};

/// A saved VIEW (name + its CREATE statement from sqlite_master).
struct ViewInfo {
    QString name;
    QString sql;
};

/// An index attached to a table.
struct IndexInfo {
    QString name;
    QString tableName;
    QStringList columns;
    bool unique = false;
    bool fromConstraint = false; // sqlite_autoindex / PRIMARY KEY backing
};

/// A trigger attached to a table.
struct TriggerInfo {
    QString name;
    QString tableName;
    QString sql;
};

/// Driver-agnostic connection descriptor. SQLite is fully wired; the server
/// fields (host/port/…) are stored so QPSQL/QMYSQL/QODBC plugins can be added
/// later without touching call sites.
struct ConnectionConfig {
    QString driverType = QStringLiteral("QSQLITE"); // QSQLITE|QPSQL|QMYSQL|QODBC
    QString filePath;        // SQLite database file
    QString host;            // server host
    QString databaseName;    // server catalog / db name
    QString userName;
    QString password;
    QString connOptions;
    int port = 0;
    QString title;           // human-friendly connection name

    bool isSqlite() const { return driverType.compare(QStringLiteral("QSQLITE"), Qt::CaseInsensitive) == 0; }
    bool isPostgres() const { return driverType.compare(QStringLiteral("QPSQL"), Qt::CaseInsensitive) == 0; }
};

/// Outcome of running one or more SQL statements.
struct QueryResult {
    bool success = false;
    bool hasResultSet = false;
    QString errorMessage;
    int affectedRows = 0;
    qint64 elapsedMs = 0;
    QSqlQuery query;        // Result set of the last SELECT-like statement.
    int failedAtStatement = -1; // 0-based index of failing statement (-1 = none).
};

/// Owns the SQLite connection and every non-UI database operation.
/// Deliberately free of any QWidget dependency (clean separation).
class DatabaseManager {
public:
    DatabaseManager();
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager &) = delete;
    DatabaseManager &operator=(const DatabaseManager &) = delete;

    // --- Connection lifecycle -----------------------------------------
    bool createDatabase(const QString &filePath, QString *error = nullptr);
    bool openDatabase(const QString &filePath, QString *error = nullptr);
    /// Driver-agnostic connect: configures the chosen Qt SQL driver + fields.
    bool connect(const ConnectionConfig &cfg, QString *error = nullptr);
    void closeDatabase();
    bool isConnected() const { return m_db.isOpen(); }
    QString currentFilePath() const { return m_filePath; }
    QString currentDatabaseName() const;
    /// Handle to the live connection (for models that re-run queries).
    QSqlDatabase database() const { return m_db; }
    /// The QSQLITE|QPSQL|... driver families this build can actually use.
    static QStringList availableDrivers();
    ConnectionConfig currentConfig() const { return m_config; }

    // --- Query execution ----------------------------------------------
    /// Splits the script into statements (quote/comment aware) and runs them.
    QueryResult runScript(const QString &sql);
    /// Runs a single statement, returns raw query for the result viewer.
    QueryResult runSingle(const QString &sql);

    /// Returns the SQLite query plan (EXPLAIN QUERY PLAN) for `sql` as a
    /// result set; the caller wraps it like any other SELECT.
    QueryResult explainQueryPlan(const QString &sql);

    // --- Schema introspection -----------------------------------------
    QStringList tableNames() const;
    QVector<ColumnInfo> columnsOf(const QString &table) const;
    QVector<ForeignKeyInfo> foreignKeysOf(const QString &table) const;

    /// Views (SQLite only; empty for other drivers for now).
    QVector<ViewInfo> views() const;
    /// Indexes + triggers for a table (SQLite; PRAGMA / sqlite_master).
    QVector<IndexInfo> indexesOf(const QString &table) const;
    QVector<TriggerInfo> triggersOf(const QString &table) const;
    /// The stored CREATE statement of any sqlite_master object (view/index/trigger).
    QString sqlFor(const QString &objectName) const;

    // --- DDL helpers (UI hooks) -----------------------------------------
    bool createTable(const TableDefinition &definition, QString *error = nullptr);
    bool addColumn(const QString &table, const ColumnSpec &column, QString *error = nullptr);
    bool dropTable(const QString &table, QString *error = nullptr);

    bool createView(const QString &name, const QString &selectSql, QString *error = nullptr);
    bool dropView(const QString &name, QString *error = nullptr);
    bool createIndex(const QString &name, const QString &table, const QStringList &columns,
                     bool unique, QString *error = nullptr);
    bool dropIndex(const QString &name, QString *error = nullptr);
    /// `ddl` is a full CREATE TRIGGER statement supplied by the caller.
    bool createTrigger(const QString &ddl, QString *error = nullptr);
    bool dropTrigger(const QString &name, QString *error = nullptr);

    /// Escapes an identifier for safe use in DDL (double quotes).
    static QString quoteIdentifier(const QString &id);

private:
    QSqlDatabase m_db;
    QString m_connectionName;
    QString m_filePath;
    ConnectionConfig m_config;
    int m_lastAffectedRows = 0;
};

} // namespace core
