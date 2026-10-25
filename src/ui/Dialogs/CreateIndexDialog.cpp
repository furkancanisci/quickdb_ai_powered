#include "ui/Dialogs/CreateIndexDialog.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

namespace ui {

CreateIndexDialog::CreateIndexDialog(const QString &table, const QStringList &columns,
                                     QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Create Index on %1").arg(table));
    setMinimumSize(420, 360);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    m_name = new QLineEdit;
    m_name->setObjectName(QStringLiteral("indexName"));
    m_name->setPlaceholderText(QStringLiteral("idx_%1 (bos = otomatik)").arg(table));
    form->addRow(QStringLiteral("Index adı:"), m_name);
    layout->addLayout(form);

    layout->addWidget(new QLabel(QStringLiteral("Kolonlar (ctrl ile çoklu seçim):")));
    m_columns = new QListWidget;
    m_columns->setObjectName(QStringLiteral("indexColumns"));
    m_columns->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_columns->setSelectionMode(QAbstractItemView::ExtendedSelection);
    for (const QString &c : columns)
        m_columns->addItem(c);
    layout->addWidget(m_columns, 1);

    m_unique = new QCheckBox(QStringLiteral("UNIQUE index"));
    m_unique->setObjectName(QStringLiteral("indexUnique"));
    layout->addWidget(m_unique);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString CreateIndexDialog::indexName() const { return m_name->text().trimmed(); }

QStringList CreateIndexDialog::selectedColumns() const
{
    QStringList out;
    const QList<QListWidgetItem *> items = m_columns->selectedItems();
    for (QListWidgetItem *it : items)
        out << it->text();
    return out;
}

bool CreateIndexDialog::isUnique() const { return m_unique->isChecked(); }

} // namespace ui
