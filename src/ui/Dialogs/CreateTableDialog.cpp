#include "ui/Dialogs/CreateTableDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace ui {

namespace {
const QStringList kSqliteTypes = {
    QStringLiteral("INTEGER"), QStringLiteral("TEXT"), QStringLiteral("REAL"),
    QStringLiteral("NUMERIC"), QStringLiteral("BLOB"), QStringLiteral("BOOLEAN"),
    QStringLiteral("DATE"), QStringLiteral("DATETIME")
};
const QStringList kPostgresTypes = {
    QStringLiteral("INTEGER"), QStringLiteral("BIGINT"), QStringLiteral("SERIAL"),
    QStringLiteral("BIGSERIAL"), QStringLiteral("TEXT"), QStringLiteral("VARCHAR"),
    QStringLiteral("NUMERIC"), QStringLiteral("REAL"), QStringLiteral("BOOLEAN"),
    QStringLiteral("DATE"), QStringLiteral("TIMESTAMP"), QStringLiteral("UUID")
};

QWidget *comboEditor(const QStringList &types)
{
    auto *combo = new QComboBox;
    combo->addItems(types);
    combo->setEditable(true);
    return combo;
}
} // namespace

CreateTableDialog::CreateTableDialog(bool isPostgres, QWidget *parent)
    : QDialog(parent)
    , m_types(isPostgres ? kPostgresTypes : kSqliteTypes)
{
    setWindowTitle(QStringLiteral("Create Table"));
    setMinimumSize(560, 380);

    auto *layout = new QVBoxLayout(this);

    auto *nameRow = new QHBoxLayout;
    nameRow->addWidget(new QLabel(QStringLiteral("Table name:")));
    m_tableName = new QLineEdit;
    m_tableName->setPlaceholderText(QStringLiteral("e.g. customers"));
    nameRow->addWidget(m_tableName);
    layout->addLayout(nameRow);

    m_grid = new QTableWidget(0, 5);
    m_grid->setHorizontalHeaderLabels(
        {QStringLiteral("Column"), QStringLiteral("Type"),
         QStringLiteral("NOT NULL"), QStringLiteral("PK"), QStringLiteral("Default")});
    m_grid->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_grid->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_grid->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_grid->verticalHeader()->setVisible(false);
    m_grid->setColumnWidth(2, 80);
    m_grid->setColumnWidth(3, 50);
    layout->addWidget(m_grid);

    auto *buttonRow = new QHBoxLayout;
    auto *addBtn = new QPushButton(QStringLiteral("+ Add Column"));
    auto *removeBtn = new QPushButton(QStringLiteral("- Remove Selected"));
    buttonRow->addWidget(addBtn);
    buttonRow->addWidget(removeBtn);
    buttonRow->addStretch();
    layout->addLayout(buttonRow);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);

    connect(addBtn, &QPushButton::clicked, this, &CreateTableDialog::addColumnRow);
    connect(removeBtn, &QPushButton::clicked, this, &CreateTableDialog::removeSelectedColumn);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Start with an "id" row, the classic SQLite PK pattern.
    addColumnRow();
    m_tableName->setFocus();
}

void CreateTableDialog::addColumnRow()
{
    const int row = m_grid->rowCount();
    m_grid->insertRow(row);

    m_grid->setItem(row, 0, new QTableWidgetItem(
        row == 0 ? QStringLiteral("id") : QString()));
    m_grid->setCellWidget(row, 1, comboEditor(m_types));

    for (int col : {2, 3}) {
        auto *check = new QCheckBox;
        // First row is the prefilled "id" -> default it to PRIMARY KEY.
        if (row == 0 && col == 3)
            check->setChecked(true);
        auto *wrap = new QWidget;
        auto *l = new QHBoxLayout(wrap);
        l->setContentsMargins(0, 0, 0, 0);
        l->setAlignment(Qt::AlignCenter);
        l->addWidget(check);
        m_grid->setCellWidget(row, col, wrap);
    }
    m_grid->setCellWidget(row, 4, new QLineEdit);
}

void CreateTableDialog::removeSelectedColumn()
{
    const int row = m_grid->currentRow();
    if (row >= 0)
        m_grid->removeRow(row);
}

core::TableDefinition CreateTableDialog::definition() const
{
    core::TableDefinition def;
    def.tableName = m_tableName->text().trimmed();

    for (int row = 0; row < m_grid->rowCount(); ++row) {
        const QTableWidgetItem *nameItem = m_grid->item(row, 0);
        const QString name = nameItem ? nameItem->text().trimmed() : QString();
        if (name.isEmpty())
            continue;

        core::ColumnSpec spec;
        spec.name = name;

        if (auto *combo = qobject_cast<QComboBox *>(m_grid->cellWidget(row, 1)))
            spec.type = combo->currentText().trimmed().toUpper();

        if (auto *wrap = qobject_cast<QWidget *>(m_grid->cellWidget(row, 2))) {
            if (auto *check = wrap->findChild<QCheckBox *>())
                spec.notNull = check->isChecked();
        }
        if (auto *wrap = qobject_cast<QWidget *>(m_grid->cellWidget(row, 3))) {
            if (auto *check = wrap->findChild<QCheckBox *>())
                spec.primaryKey = check->isChecked();
        }
        if (auto *edit = qobject_cast<QLineEdit *>(m_grid->cellWidget(row, 4)))
            spec.defaultValue = edit->text().trimmed();

        def.columns.append(spec);
    }
    return def;
}

} // namespace ui
