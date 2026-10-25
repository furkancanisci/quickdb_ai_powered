#include "ui/Dialogs/CreateViewDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QVBoxLayout>

namespace ui {

CreateViewDialog::CreateViewDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Create View"));
    setMinimumSize(480, 320);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    m_name = new QLineEdit;
    m_name->setObjectName(QStringLiteral("viewName"));
    m_name->setPlaceholderText(QStringLiteral("view_adi"));
    form->addRow(QStringLiteral("Görünüm adı:"), m_name);
    layout->addLayout(form);

    layout->addWidget(new QLabel(QStringLiteral("SELECT sorgusu:")));
    m_select = new QPlainTextEdit;
    m_select->setObjectName(QStringLiteral("viewSelect"));
    m_select->setPlaceholderText(QStringLiteral("SELECT * FROM tabela WHERE ..."));
    layout->addWidget(m_select, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString CreateViewDialog::viewName() const { return m_name->text().trimmed(); }
QString CreateViewDialog::selectSql() const { return m_select->toPlainText().trimmed(); }

} // namespace ui
