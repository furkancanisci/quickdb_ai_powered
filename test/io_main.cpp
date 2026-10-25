// Console round-trip test for the export/import IO layer (no GUI, no DB).
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTemporaryDir>

#include "core/io/Exporters.h"
#include "core/io/Importers.h"

static int failures = 0;
static void check(bool cond, const QString &label)
{
    qInfo().noquote() << (cond ? QStringLiteral("[PASS] ") : QStringLiteral("[FAIL] ")) + label;
    if (!cond)
        ++failures;
}

static core::io::ExportInput sample()
{
    core::io::ExportInput in;
    in.tableName = QStringLiteral("kisiler");
    in.columns = {QStringLiteral("id"), QStringLiteral("ad"), QStringLiteral("sehir")};
    in.rows = {
        {QStringLiteral("1"), QStringLiteral("Ali"), QStringLiteral("Istanbul")},
        {QStringLiteral("2"), QStringLiteral("Ayse, K."), QStringLiteral("Ankara")},
        {QStringLiteral("3"), QStringLiteral("has\"quote"), QStringLiteral("Izmir")},
    };
    return in;
}

static bool writeText(const QString &path, const QString &text)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    f.write(text.toUtf8());
    f.close();
    return true;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;

    const core::io::ExportInput in = sample();

    // --- CSV export ---------------------------------------------------
    const QString csv = core::io::toCsv(in);
    check(csv.contains(QStringLiteral("id,ad,sehir")), QStringLiteral("csv header"));
    check(csv.contains(QStringLiteral("\"Ayse, K.\"")), QStringLiteral("csv quotes comma field"));
    check(csv.contains(QStringLiteral("\"has\"\"quote\"")), QStringLiteral("csv escapes inner quote"));

    // --- JSON export --------------------------------------------------
    const QString json = core::io::toJson(in);
    check(json.contains(QStringLiteral("\"ad\"")), QStringLiteral("json keys present"));
    check(json.contains(QStringLiteral("Ayse, K.")), QStringLiteral("json value present"));

    // --- XML export ---------------------------------------------------
    const QString xml = core::io::toXml(in);
    check(xml.contains(QStringLiteral("<table")), QStringLiteral("xml root"));
    check(xml.contains(QStringLiteral("name=\"kisiler\"")), QStringLiteral("xml table name attr"));
    check(xml.contains(QStringLiteral("<row>")), QStringLiteral("xml row elements"));

    // --- SQL dump export ----------------------------------------------
    const QString sql = core::io::toSqlDump(in);
    check(sql.contains(QStringLiteral("BEGIN TRANSACTION;")), QStringLiteral("sql dump opens txn"));
    check(sql.contains(QStringLiteral("INSERT INTO \"kisiler\"")), QStringLiteral("sql dump insert"));
    check(sql.contains(QStringLiteral("COMMIT;")), QStringLiteral("sql dump closes txn"));

    // --- CSV round-trip -----------------------------------------------
    const QString csvPath = dir.path() + QStringLiteral("/in.csv");
    check(writeText(csvPath, csv), QStringLiteral("write csv file"));
    const core::io::ImportedTable fromCsv = core::io::parseCsv(csvPath);
    check(fromCsv.ok, QStringLiteral("csv parse ok"));
    check(fromCsv.columns == in.columns, QStringLiteral("csv columns round-trip"));
    check(fromCsv.rowCount() == in.rows.size(), QStringLiteral("csv row count round-trip"));
    check(fromCsv.rows[1][1] == QStringLiteral("Ayse, K."), QStringLiteral("csv comma field round-trip"));
    check(fromCsv.rows[2][1] == QStringLiteral("has\"quote"), QStringLiteral("csv quote field round-trip"));

    // --- JSON round-trip ----------------------------------------------
    const QString jsonPath = dir.path() + QStringLiteral("/in.json");
    check(writeText(jsonPath, json), QStringLiteral("write json file"));
    const core::io::ImportedTable fromJson = core::io::parseJson(jsonPath);
    check(fromJson.ok, QStringLiteral("json parse ok"));
    check(fromJson.rowCount() == in.rows.size(), QStringLiteral("json row count round-trip"));
    check(fromJson.columns.contains(QStringLiteral("sehir")), QStringLiteral("json columns recovered"));

    // --- Type inference + DDL -----------------------------------------
    core::io::ImportedTable typed;
    typed.columns = {QStringLiteral("n"), QStringLiteral("r"), QStringLiteral("s")};
    typed.rows = {
        {QStringLiteral("10"), QStringLiteral("1.5"), QStringLiteral("hello")},
        {QStringLiteral("20"), QStringLiteral("2.25"), QStringLiteral("world")},
    };
    check(core::io::inferColumnType(typed, 0) == QStringLiteral("INTEGER"), QStringLiteral("infer INTEGER"));
    check(core::io::inferColumnType(typed, 1) == QStringLiteral("REAL"), QStringLiteral("infer REAL"));
    check(core::io::inferColumnType(typed, 2) == QStringLiteral("TEXT"), QStringLiteral("infer TEXT"));
    const QString ddl = core::io::buildCreateTable(QStringLiteral("t"), typed);
    check(ddl.startsWith(QStringLiteral("CREATE TABLE")), QStringLiteral("buildCreateTable"));
    const QString inserts = core::io::buildInserts(QStringLiteral("t"), typed);
    check(inserts.contains(QStringLiteral("INSERT INTO \"t\"")), QStringLiteral("buildInserts"));

    qInfo().noquote() << (failures == 0 ? QStringLiteral("IO TEST: ALL PASSED")
                                        : QStringLiteral("IO TEST: %1 FAILED").arg(failures));
    return failures == 0 ? 0 : 1;
}
