#pragma once

#include <QDialog>
#include <QStringList>
#include "core/DatabaseManager.h"

class QLineEdit;
class QTableWidget;

namespace ui {

/// Visual DDL builder: table name + column grid (name, type, NOT NULL, PK,
/// default). Produces a core::TableDefinition consumed by DatabaseManager.
class CreateTableDialog : public QDialog {
    Q_OBJECT

public:
    explicit CreateTableDialog(bool isPostgres = false, QWidget *parent = nullptr);

    core::TableDefinition definition() const;

private slots:
    void addColumnRow();
    void removeSelectedColumn();

private:
    QLineEdit *m_tableName = nullptr;
    QTableWidget *m_grid = nullptr;
    QStringList m_types; // SQLite or PostgreSQL column-type candidates
};

} // namespace ui
