#include "ui/Dialogs/AddColumnDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QVBoxLayout>

namespace ui {

AddColumnDialog::AddColumnDialog(const QStringList &tables, bool isPostgres, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Add Column"));
    setMinimumWidth(360);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;

    m_table = new QComboBox;
    m_table->addItems(tables);
    m_table->setEditable(tables.isEmpty());
    form->addRow(QStringLiteral("Table:"), m_table);

    m_name = new QLineEdit;
    m_name->setPlaceholderText(QStringLiteral("column name"));
    form->addRow(QStringLiteral("Column name:"), m_name);

    m_type = new QComboBox;
    if (isPostgres)
        m_type->addItems({QStringLiteral("INTEGER"), QStringLiteral("BIGINT"),
                          QStringLiteral("SERIAL"), QStringLiteral("BIGSERIAL"),
                          QStringLiteral("TEXT"), QStringLiteral("VARCHAR"),
                          QStringLiteral("NUMERIC"), QStringLiteral("REAL"),
                          QStringLiteral("BOOLEAN"), QStringLiteral("DATE"),
                          QStringLiteral("TIMESTAMP"), QStringLiteral("UUID")});
    else
        m_type->addItems({QStringLiteral("INTEGER"), QStringLiteral("TEXT"),
                          QStringLiteral("REAL"), QStringLiteral("NUMERIC"),
                          QStringLiteral("BLOB"), QStringLiteral("BOOLEAN"),
                          QStringLiteral("DATE"), QStringLiteral("DATETIME")});
    m_type->setEditable(true);
    form->addRow(QStringLiteral("Type:"), m_type);

    m_default = new QLineEdit;
    m_default->setPlaceholderText(QStringLiteral("optional, e.g. 0 or 'n/a'"));
    form->addRow(QStringLiteral("Default:"), m_default);

    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    m_name->setFocus();
}

QString AddColumnDialog::targetTable() const
{
    return m_table->currentText().trimmed();
}

void AddColumnDialog::setCurrentTableIndex(int index)
{
    m_table->setCurrentIndex(index);
}

core::ColumnSpec AddColumnDialog::columnSpec() const
{
    core::ColumnSpec spec;
    spec.name = m_name->text().trimmed();
    spec.type = m_type->currentText().trimmed().toUpper();
    spec.defaultValue = m_default->text().trimmed();
    return spec;
}

} // namespace ui
