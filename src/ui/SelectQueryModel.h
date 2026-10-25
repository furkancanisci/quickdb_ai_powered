#pragma once

#include <QSqlDatabase>
#include <QSqlQueryModel>
#include <QString>

namespace ui {

/// QSqlQueryModel subclass that renders BLOB values as a size summary
/// instead of dumping raw bytes into the grid, and greys out NULL cells.
class SelectQueryModel : public QSqlQueryModel {
    Q_OBJECT

public:
    explicit SelectQueryModel(QObject *parent = nullptr);

    QVariant data(const QModelIndex &item, int role = Qt::DisplayRole) const override;

    /// Takes ownership of an executed SELECT query and remembers its SQL +
    /// connection so sort() can re-execute it with ORDER BY.
    void setSource(QSqlQuery &&query, const QSqlDatabase &db);

    /// Re-executes the source SQL wrapped in an ORDER BY so clickable
    /// column headers really sort the grid (QSqlQueryModel::sort is a no-op).
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    /// Clears the model and releases the underlying query so the database
    /// connection can be safely closed/replaced.
    void clearAll();

private:
    QSqlDatabase m_db;      // connection the source query ran on
    QString m_sourceSql;    // SQL of the last setSource() query
};

} // namespace ui
