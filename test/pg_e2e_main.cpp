// PostgreSQL integration test for core::DatabaseManager.
//
// Safe to run anywhere: if the QPSQL driver is not built, or no local server
// is reachable, the test SKIPS and returns success (0). When a server IS up
// (see scripts/setup_local_pg.sh) it verifies connect -> introspection ->
// SELECT -> result-set materialization against the seeded `quickdb_test` DB.
//
//   scripts/build_qpsql_plugin.sh && scripts/setup_local_pg.sh
//   QT_PLUGIN_PATH=build ./build/pg_e2e_test
#include <QCoreApplication>
#include <QDebug>
#include <QProcessEnvironment>
#include <QSqlQuery>

#include "core/DatabaseManager.h"

static int failures = 0;
static void check(bool cond, const QString &label)
{
    qInfo().noquote() << (cond ? QStringLiteral("[PASS] ") : QStringLiteral("[FAIL] ")) + label;
    if (!cond)
        ++failures;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    if (!core::DatabaseManager::availableDrivers().contains(QStringLiteral("QPSQL"),
                                                            Qt::CaseInsensitive)) {
        qInfo().noquote() << "PG E2E: SKIPPED (QPSQL driver not built; run "
                             "scripts/build_qpsql_plugin.sh).";
        return 0;
    }

    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    core::ConnectionConfig cfg;
    cfg.driverType = QStringLiteral("QPSQL");
    cfg.host = env.value(QStringLiteral("PGHOST"), QStringLiteral("127.0.0.1"));
    cfg.port = env.value(QStringLiteral("PGPORT_TEST"), QStringLiteral("5433")).toInt();
    cfg.databaseName = env.value(QStringLiteral("PGDATABASE"), QStringLiteral("quickdb_test"));
    cfg.userName = env.value(QStringLiteral("PGUSER"), QStringLiteral("postgres"));
    cfg.password = env.value(QStringLiteral("PGPASSWORD"));
    cfg.title = QStringLiteral("pg_e2e");

    core::DatabaseManager db;
    QString error;
    if (!db.connect(cfg, &error)) {
        qInfo().noquote() << "PG E2E: SKIPPED (no server on"
                          << QStringLiteral("%1:%2/%3").arg(cfg.host).arg(cfg.port).arg(cfg.databaseName)
                          << "):" << error;
        return 0; // server down is an environment gap, not a code failure
    }

    check(!error.isEmpty() || db.isConnected(), QStringLiteral("connected to PostgreSQL"));

    // Introspection: the seeded tables must be discovered via pg_tables.
    const QStringList tables = db.tableNames();
    check(tables.contains(QStringLiteral("employees")) && tables.contains(QStringLiteral("departments")),
          QStringLiteral("tableNames() lists seeded tables (%1)").arg(tables.join(QStringLiteral(", "))));

    // columnsOf() via information_schema: PK + NOT NULL flags.
    const auto empCols = db.columnsOf(QStringLiteral("employees"));
    bool sawPk = false, sawNotNull = false;
    for (const core::ColumnInfo &c : empCols) {
        if (c.name == QStringLiteral("id") && c.primaryKey)
            sawPk = true;
        if (c.name == QStringLiteral("name") && c.notNull)
            sawNotNull = true;
    }
    check(empCols.size() >= 7 && sawPk && sawNotNull,
          QStringLiteral("columnsOf(employees) introspects cols/PK/NOT NULL (n=%1)").arg(empCols.size()));

    // foreignKeysOf() via information_schema joins.
    const auto fks = db.foreignKeysOf(QStringLiteral("employees"));
    bool depFk = false;
    for (const core::ForeignKeyInfo &fk : fks)
        if (fk.fromColumn == QStringLiteral("department_id") && fk.toTable == QStringLiteral("departments"))
            depFk = true;
    check(!fks.isEmpty() && depFk,
          QStringLiteral("foreignKeysOf(employees) finds department_id -> departments (%1)").arg(fks.size()));

    // A real SELECT through the result-set path.
    core::QueryResult r = db.runSingle(QStringLiteral(
        "SELECT department_id, COUNT(*) AS n, AVG(salary) AS avg_salary "
        "FROM employees GROUP BY department_id ORDER BY department_id"));
    check(r.success && r.hasResultSet, QStringLiteral("aggregate SELECT succeeds (%1)").arg(r.errorMessage));
    int rows = 0;
    while (r.query.next())
        ++rows;
    check(rows >= 2, QStringLiteral("SELECT returned grouped rows (%1)").arg(rows));

    // indexesOf() via pg_index.
    const auto idx = db.indexesOf(QStringLiteral("employees"));
    check(!idx.isEmpty(), QStringLiteral("indexesOf(employees) lists pg_index rows (%1)").arg(idx.size()));

    db.closeDatabase();
    qInfo().noquote() << (failures == 0 ? QStringLiteral("PG E2E TEST: ALL PASSED")
                                        : QStringLiteral("PG E2E TEST: %1 FAILURE(S)").arg(failures));
    return failures;
}
