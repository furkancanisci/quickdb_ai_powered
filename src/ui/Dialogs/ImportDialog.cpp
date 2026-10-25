#include "ui/Dialogs/ImportDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace ui {

ImportDialog::ImportDialog(core::DatabaseManager *db, QWidget *parent)
    : QDialog(parent), m_db(db)
{
    setWindowTitle(QStringLiteral("Import File"));
    setMinimumSize(560, 480);

    auto *layout = new QVBoxLayout(this);

    auto *form = new QFormLayout;
    auto *fileRow = new QHBoxLayout;
    m_file = new QLineEdit;
    m_file->setObjectName(QStringLiteral("importFile"));
    m_file->setPlaceholderText(QStringLiteral("CSV / JSON dosyası…"));
    m_browse = new QPushButton(QStringLiteral("Gözat…"));
    m_browse->setObjectName(QStringLiteral("importBrowse"));
    fileRow->addWidget(m_file, 1);
    fileRow->addWidget(m_browse);
    form->addRow(QStringLiteral("Dosya:"), fileRow);

    m_delim = new QComboBox;
    m_delim->setObjectName(QStringLiteral("importDelim"));
    m_delim->addItem(QStringLiteral("Virgül ( , )"), QStringLiteral(","));
    m_delim->addItem(QStringLiteral("Noktalı virgül ( ; )"), QStringLiteral(";"));
    m_delim->addItem(QStringLiteral("Tab"), QStringLiteral("\t"));
    m_delim->addItem(QStringLiteral("Dikey çizgi ( | )"), QStringLiteral("|"));
    form->addRow(QStringLiteral("Ayırıcı:"), m_delim);

    m_header = new QCheckBox(QStringLiteral("İlk satır başlık (kolon adları)"));
    m_header->setObjectName(QStringLiteral("importHeader"));
    m_header->setChecked(true);
    form->addRow(QString(), m_header);

    m_table = new QLineEdit;
    m_table->setObjectName(QStringLiteral("importTable"));
    m_table->setPlaceholderText(QStringLiteral("Hedef tablo adı"));
    form->addRow(QStringLiteral("Hedef tablo:"), m_table);
    layout->addLayout(form);

    auto *loadBtn = new QPushButton(QStringLiteral("Önizle"));
    loadBtn->setObjectName(QStringLiteral("importLoad"));
    layout->addWidget(loadBtn);

    m_preview = new QTableWidget;
    m_preview->setObjectName(QStringLiteral("importPreview"));
    m_preview->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_preview->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(m_preview, 1);

    m_status = new QLabel;
    m_status->setObjectName(QStringLiteral("importStatus"));
    m_status->setStyleSheet(QStringLiteral("color:#667085;"));
    layout->addWidget(m_status);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("İçe Aktar"));
    layout->addWidget(buttons);

    connect(m_browse, &QPushButton::clicked, this, &ImportDialog::browse);
    connect(loadBtn, &QPushButton::clicked, this, &ImportDialog::onLoadClicked);
    connect(buttons, &QDialogButtonBox::accepted, this, &ImportDialog::onImportClicked);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QChar ImportDialog::currentDelimiter() const
{
    const QString d = m_delim->currentData().toString();
    return d.isEmpty() ? QChar(QLatin1Char(',')) : d.at(0);
}

void ImportDialog::browse()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("İçe Aktar"), QDir::homePath(),
        QStringLiteral("Veri Dosyaları (*.csv *.json *.txt);;All Files (*)"),
        nullptr, QFileDialog::DontUseNativeDialog);
    if (path.isEmpty())
        return;
    m_file->setText(path);
    // Suggest a table name from the file stem.
    const QFileInfo fi(path);
    QString stem = fi.completeBaseName();
    stem.replace(QLatin1Char(' '), QLatin1Char('_'));
    if (!stem.isEmpty() && m_table->text().isEmpty())
        m_table->setText(stem);
}

QString ImportDialog::targetTable() const
{
    return m_table->text().trimmed();
}

QString ImportDialog::loadPreview()
{
    const QString path = m_file->text().trimmed();
    if (path.isEmpty())
        return QStringLiteral("Önce bir dosya seçin.");
    if (!QFile::exists(path))
        return QStringLiteral("Dosya bulunamadı: %1").arg(path);

    if (path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
        m_parsed = core::io::parseJson(path);
    else
        m_parsed = core::io::parseCsv(path, currentDelimiter(), m_header->isChecked());

    if (!m_parsed.ok)
        return m_parsed.error;

    fillPreview();
    m_status->setStyleSheet(QStringLiteral("color:#2e7d32;"));
    m_status->setText(QStringLiteral("✔ %1 kolon, %2 satır önizlendi.")
                          .arg(m_parsed.columnCount()).arg(m_parsed.rowCount()));
    return QString();
}

void ImportDialog::fillPreview()
{
    m_preview->clear();
    m_preview->setColumnCount(m_parsed.columnCount());
    m_preview->setRowCount(m_parsed.rowCount());
    QStringList headers;
    for (const QString &c : m_parsed.columns)
        headers << c;
    m_preview->setHorizontalHeaderLabels(headers);
    for (int r = 0; r < m_parsed.rowCount(); ++r) {
        for (int c = 0; c < m_parsed.columnCount(); ++c) {
            const QString v = c < m_parsed.rows[r].size() ? m_parsed.rows[r][c] : QString();
            m_preview->setItem(r, c, new QTableWidgetItem(v));
        }
    }
}

QString ImportDialog::runImport()
{
    if (!m_db || !m_db->isConnected())
        return QStringLiteral("Bağlı bir veritabanı yok.");
    if (!m_parsed.ok) {
        const QString err = loadPreview();
        if (!err.isEmpty())
            return err;
    }
    const QString table = targetTable();
    if (table.isEmpty())
        return QStringLiteral("Hedef tablo adı gerekli.");
    if (m_parsed.columnCount() == 0)
        return QStringLiteral("İçe aktarılacak kolon yok.");

    const bool exists = m_db->tableNames().contains(table, Qt::CaseInsensitive);
    QString script;
    if (!exists)
        script += core::io::buildCreateTable(table, m_parsed) + QLatin1Char('\n');
    script += QStringLiteral("BEGIN TRANSACTION;\n");
    script += core::io::buildInserts(table, m_parsed) + QLatin1Char('\n');
    script += QStringLiteral("COMMIT;\n");

    const core::QueryResult res = m_db->runScript(script);
    if (!res.success) {
        m_status->setStyleSheet(QStringLiteral("color:#b71c1c;"));
        m_status->setText(QStringLiteral("✖ %1").arg(res.errorMessage));
        return res.errorMessage;
    }
    m_status->setStyleSheet(QStringLiteral("color:#2e7d32;"));
    m_status->setText(QStringLiteral("✔ %1 satır '%2' tablosuna aktarıldı.")
                          .arg(m_parsed.rowCount()).arg(table));
    return QString();
}

void ImportDialog::onLoadClicked()
{
    const QString err = loadPreview();
    if (!err.isEmpty()) {
        m_status->setStyleSheet(QStringLiteral("color:#b71c1c;"));
        m_status->setText(QStringLiteral("✖ %1").arg(err));
    }
}

void ImportDialog::onImportClicked()
{
    const QString err = runImport();
    if (err.isEmpty())
        accept();
}

} // namespace ui
