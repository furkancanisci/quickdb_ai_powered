#include "ui/SelectQueryModel.h"

#include <QByteArray>
#include <QColor>
#include <QSqlQuery>
#include <QSqlRecord>

namespace ui {

SelectQueryModel::SelectQueryModel(QObject *parent)
    : QSqlQueryModel(parent)
{
}

QVariant SelectQueryModel::data(const QModelIndex &item, int role) const
{
    if (!item.isValid())
        return QVariant();

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        const QVariant value = QSqlQueryModel::data(item, role);
        if (value.typeId() == QMetaType::QByteArray) {
            const QByteArray blob = value.toByteArray();
            return QStringLiteral("(BLOB · %1 bytes)").arg(blob.size());
        }
        return value;
    }

    if (role == Qt::ForegroundRole) {
        const QVariant value = QSqlQueryModel::data(item, Qt::DisplayRole);
        if (!value.isValid() || value.isNull())
            return QColor(160, 160, 160);
    }

    return QSqlQueryModel::data(item, role);
}

void SelectQueryModel::setSource(QSqlQuery &&query, const QSqlDatabase &db)
{
    m_sourceSql = query.lastQuery();
    m_db = db;
    setQuery(std::move(query));
}

void SelectQueryModel::sort(int column, Qt::SortOrder order)
{
    if (rowCount() == 0 || column < 0 || column >= record().count())
        return;

    const QString baseSql = m_sourceSql.trimmed().remove(QLatin1Char(';'));
    if (baseSql.isEmpty() || !m_db.isOpen())
        return;

    const QString field = QLatin1Char('"') +
                          record().fieldName(column).replace(QLatin1Char('"'), QLatin1String("\"\"")) +
                          QLatin1Char('"');
    const QString sorted = QStringLiteral(
                               "SELECT * FROM (%1) AS _quickdb_sort ORDER BY _quickdb_sort.%2 %3;")
                               .arg(baseSql,
                                    field,
                                    order == Qt::AscendingOrder ? QStringLiteral("ASC")
                                                                : QStringLiteral("DESC"));

    QSqlQuery reordered(m_db);
    if (!reordered.exec(sorted))
        return; // keep current order on failure (e.g. non-selectable source)

    setQuery(std::move(reordered));
}

void SelectQueryModel::clearAll()
{
    clear(); // drops the underlying query and resets the model
    m_db = QSqlDatabase();  // release connection handle copies
    m_sourceSql.clear();
}

} // namespace ui
