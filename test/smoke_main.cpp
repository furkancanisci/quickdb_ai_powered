// Console smoke test for core::DatabaseManager (no GUI, links Qt Sql only).
#include <QCoreApplication>
#include <QDebug>
#include <QTemporaryDir>

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
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/smoke.db");

    core::DatabaseManager db;
    QString error;

    check(db.createDatabase(path, &error), QStringLiteral("create database (%1)").arg(error));

    core::TableDefinition def;
    def.tableName = QStringLiteral("users");
    def.columns = {
        {QStringLiteral("id"), QStringLiteral("INTEGER"), false, true, QString()},
        {QStringLiteral("name"), QStringLiteral("TEXT"), true, false, QString()},
        {QStringLiteral("age"), QStringLiteral("INTEGER"), false, false, QStringLiteral("0")}
    };
    check(db.createTable(def, &error), QStringLiteral("create table users (%1)").arg(error));

    check(db.runScript(QStringLiteral(
              "INSERT INTO users (name) VALUES ('furkan');\n"
              "INSERT INTO users (name, age) VALUES ('ali', 30);")).success,
          QStringLiteral("multi-statement INSERT"));

    auto sel = db.runScript(QStringLiteral("SELECT id, name, age FROM users ORDER BY id;"));
    check(sel.success && sel.hasResultSet, QStringLiteral("SELECT returns result set"));
    int rows = 0;
    while (sel.query.next())
        ++rows;
    check(rows == 2, QStringLiteral("SELECT row count == 2 (got %1)").arg(rows));

    check(db.tableNames() == QStringList{QStringLiteral("users")}, QStringLiteral("schema: tableNames"));
    const auto cols = db.columnsOf(QStringLiteral("users"));
    check(cols.size() == 3 && cols.at(0).primaryKey && cols.at(1).notNull,
          QStringLiteral("schema: columnsOf flags"));

    check(db.addColumn(QStringLiteral("users"),
                       {QStringLiteral("email"), QStringLiteral("TEXT"), false, false, QString()},
                       &error),
          QStringLiteral("alter table add column (%1)").arg(error));

    auto bad = db.runScript(QStringLiteral("SELECT * FROM no_such_table;"));
    check(!bad.success && !bad.errorMessage.isEmpty(),
          QStringLiteral("error handling: missing table -> %1").arg(bad.errorMessage));

    check(db.runScript(QStringLiteral("UPDATE users SET age = 31 WHERE name = 'ali';")).success,
          QStringLiteral("UPDATE"));
    check(db.runScript(QStringLiteral("DELETE FROM users WHERE name = 'ali';")).affectedRows == 1,
          QStringLiteral("DELETE affected 1 row"));

    // --- Phase G: EXPLAIN QUERY PLAN --------------------------------
    auto plan = db.explainQueryPlan(QStringLiteral("SELECT * FROM users"));
    check(plan.success && plan.hasResultSet, QStringLiteral("explainQueryPlan returns result set"));
    int planRows = 0;
    while (plan.query.next())
        ++planRows;
    check(planRows >= 1, QStringLiteral("explain plan has rows (got %1)").arg(planRows));

    // --- Phase F: views / indexes / triggers --------------------------
    check(db.createView(QStringLiteral("v_users"),
                        QStringLiteral("SELECT id, name FROM users"), &error),
          QStringLiteral("create view (%1)").arg(error));
    const auto views = db.views();
    check(views.size() == 1 && views.at(0).name == QStringLiteral("v_users"),
          QStringLiteral("introspect: views() finds v_users"));
    check(db.sqlFor(QStringLiteral("v_users")).contains(QStringLiteral("SELECT"), Qt::CaseInsensitive),
          QStringLiteral("sqlFor returns view DDL"));

    check(db.createIndex(QStringLiteral("idx_users_name"), QStringLiteral("users"),
                         {QStringLiteral("name")}, false, &error),
          QStringLiteral("create index (%1)").arg(error));
    const auto idxs = db.indexesOf(QStringLiteral("users"));
    bool foundIdx = false;
    for (const auto &ix : idxs)
        if (ix.name == QStringLiteral("idx_users_name"))
            foundIdx = true;
    check(foundIdx, QStringLiteral("introspect: indexesOf finds idx_users_name"));

    check(db.createTrigger(QStringLiteral(
              "CREATE TRIGGER trg_no_delete BEFORE DELETE ON users BEGIN SELECT 1; END;"), &error),
          QStringLiteral("create trigger (%1)").arg(error));
    const auto trgs = db.triggersOf(QStringLiteral("users"));
    check(trgs.size() == 1 && trgs.at(0).name == QStringLiteral("trg_no_delete"),
          QStringLiteral("introspect: triggersOf finds trg_no_delete"));

    // Deleting a user now hits the (no-op) trigger; DELETE still succeeds.
    check(db.runScript(QStringLiteral("DELETE FROM users WHERE name = 'furkan';")).success,
          QStringLiteral("delete with trigger present succeeds"));

    check(db.dropTrigger(QStringLiteral("trg_no_delete"), &error),
          QStringLiteral("drop trigger (%1)").arg(error));
    check(db.dropIndex(QStringLiteral("idx_users_name"), &error),
          QStringLiteral("drop index (%1)").arg(error));
    check(db.dropView(QStringLiteral("v_users"), &error),
          QStringLiteral("drop view (%1)").arg(error));
    check(db.views().isEmpty(), QStringLiteral("view dropped"));

    check(db.dropTable(QStringLiteral("users"), &error), QStringLiteral("drop table (%1)").arg(error));

    db.closeDatabase();
    check(!db.isConnected(), QStringLiteral("close database"));

    qInfo().noquote() << (failures == 0 ? QStringLiteral("ALL TESTS PASSED")
                                        : QStringLiteral("%1 TEST(S) FAILED").arg(failures));
    return failures;
}
