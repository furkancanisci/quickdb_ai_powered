#include "ui/Dialogs/ExportDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace ui {

ExportDialog::ExportDialog(const core::io::ExportInput &data, QWidget *parent)
    : QDialog(parent), m_data(data)
{
    setWindowTitle(QStringLiteral("Export Result"));
    setMinimumSize(520, 460);

    auto *layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(QStringLiteral("%1 kolon, %2 satır")
                                     .arg(m_data.columns.size()).arg(m_data.rows.size())));

    auto *form = new QFormLayout;
    m_format = new QComboBox;
    m_format->setObjectName(QStringLiteral("exportFormat"));
    m_format->addItem(QStringLiteral("CSV"), QStringLiteral("csv"));
    m_format->addItem(QStringLiteral("JSON"), QStringLiteral("json"));
    m_format->addItem(QStringLiteral("XML"), QStringLiteral("xml"));
    m_format->addItem(QStringLiteral("SQL Dump"), QStringLiteral("sql"));
    form->addRow(QStringLiteral("Format:"), m_format);

    auto *fileRow = new QHBoxLayout;
    m_file = new QLineEdit;
    m_file->setObjectName(QStringLiteral("exportFile"));
    m_file->setPlaceholderText(QStringLiteral("Hedef dosya…"));
    m_browse = new QPushButton(QStringLiteral("Gözat…"));
    m_browse->setObjectName(QStringLiteral("exportBrowse"));
    fileRow->addWidget(m_file, 1);
    fileRow->addWidget(m_browse);
    form->addRow(QStringLiteral("Dosya:"), fileRow);
    layout->addLayout(form);

    layout->addWidget(new QLabel(QStringLiteral("Önizleme:")));
    m_preview = new QPlainTextEdit;
    m_preview->setObjectName(QStringLiteral("exportPreview"));
    m_preview->setReadOnly(true);
    m_preview->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(m_preview, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("Dışa Aktar"));
    layout->addWidget(buttons);

    connect(m_format, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ExportDialog::refreshPreview);
    connect(m_browse, &QPushButton::clicked, this, &ExportDialog::browse);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    refreshPreview();
}

QString ExportDialog::format() const
{
    return m_format->currentData().toString();
}

QString ExportDialog::rendered() const
{
    const QString f = format();
    if (f == QStringLiteral("json"))
        return core::io::toJson(m_data);
    if (f == QStringLiteral("xml"))
        return core::io::toXml(m_data);
    if (f == QStringLiteral("sql"))
        return core::io::toSqlDump(m_data);
    return core::io::toCsv(m_data);
}

QString ExportDialog::filePath() const
{
    return m_file->text().trimmed();
}

void ExportDialog::refreshPreview()
{
    m_preview->setPlainText(rendered());
}

void ExportDialog::browse()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Dışa Aktar"), QString(),
        QStringLiteral("All Files (*)"), nullptr, QFileDialog::DontUseNativeDialog);
    if (!path.isEmpty())
        m_file->setText(path);
}

QString ExportDialog::writeTo(const QString &path)
{
    QString target = path.isEmpty() ? filePath() : path;
    if (target.isEmpty())
        return QStringLiteral("No target file selected.");
    QFile f(target);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return f.errorString();
    const QByteArray data = rendered().toUtf8();
    f.write(data);
    f.close();
    return QString();
}

} // namespace ui
