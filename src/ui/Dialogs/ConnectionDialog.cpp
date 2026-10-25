#include "ui/Dialogs/ConnectionDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

namespace ui {

namespace {
QSettings &settings()
{
    static QSettings s(QStringLiteral("QuickDb"), QStringLiteral("QuickDb"));
    return s;
}
} // namespace

ConnectionDialog::ConnectionDialog(const core::ConnectionConfig &initial, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Connection Manager"));
    setMinimumWidth(440);
    buildUi();

    // Apply the incoming config (or a sensible default).
    m_title->setText(initial.title.isEmpty() ? QStringLiteral("Local SQLite") : initial.title);
    m_filePath->setText(initial.filePath);
    m_host->setText(initial.host);
    m_port->setValue(initial.port > 0 ? initial.port : 5432);
    m_dbName->setText(initial.databaseName);
    m_user->setText(initial.userName);
    m_pass->setText(initial.password);
    const int d = m_driver->findData(initial.driverType);
    m_driver->setCurrentIndex(d >= 0 ? d : 0);
    onDriverChanged(m_driver->currentIndex());
}

bool ConnectionDialog::available(const QString &driver) const
{
    return core::DatabaseManager::availableDrivers().contains(driver, Qt::CaseInsensitive);
}

void ConnectionDialog::buildUi()
{
    auto *layout = new QVBoxLayout(this);

    // --- Saved connections row -----------------------------------------
    auto *savedRow = new QHBoxLayout;
    m_saved = new QComboBox;
    m_saved->setObjectName(QStringLiteral("connSaved"));
    m_saved->setMinimumWidth(180);
    auto *saveBtn = new QPushButton(QStringLiteral("Kaydet"));
    saveBtn->setObjectName(QStringLiteral("connSave"));
    savedRow->addWidget(new QLabel(QStringLiteral("Kayıtlı:")));
    savedRow->addWidget(m_saved, 1);
    savedRow->addWidget(saveBtn);
    layout->addLayout(savedRow);

    auto *form = new QFormLayout;
    m_title = new QLineEdit;
    m_title->setObjectName(QStringLiteral("connTitle"));
    form->addRow(QStringLiteral("Bağlantı adı:"), m_title);

    m_driver = new QComboBox;
    m_driver->setObjectName(QStringLiteral("connDriver"));
    m_driver->addItem(QStringLiteral("SQLite (dosya)"), QStringLiteral("QSQLITE"));
    if (available(QStringLiteral("QPSQL")))
        m_driver->addItem(QStringLiteral("PostgreSQL"), QStringLiteral("QPSQL"));
    if (available(QStringLiteral("QMYSQL")))
        m_driver->addItem(QStringLiteral("MySQL"), QStringLiteral("QMYSQL"));
    if (available(QStringLiteral("QODBC")))
        m_driver->addItem(QStringLiteral("ODBC"), QStringLiteral("QODBC"));
    form->addRow(QStringLiteral("Sürücü:"), m_driver);
    layout->addLayout(form);

    // --- SQLite group ---------------------------------------------------
    m_sqliteGroup = new QGroupBox(QStringLiteral("SQLite dosyası"));
    auto *sqliteForm = new QFormLayout(m_sqliteGroup);
    auto *fileRow = new QHBoxLayout;
    m_filePath = new QLineEdit;
    m_filePath->setObjectName(QStringLiteral("connFilePath"));
    m_filePath->setPlaceholderText(QStringLiteral("~/veri.db"));
    m_browse = new QPushButton(QStringLiteral("Gözat…"));
    m_browse->setObjectName(QStringLiteral("connBrowse"));
    fileRow->addWidget(m_filePath, 1);
    fileRow->addWidget(m_browse);
    sqliteForm->addRow(QStringLiteral("Dosya:"), fileRow);
    layout->addWidget(m_sqliteGroup);

    // --- Server group ---------------------------------------------------
    m_serverGroup = new QGroupBox(QStringLiteral("Sunucu"));
    auto *serverForm = new QFormLayout(m_serverGroup);
    m_host = new QLineEdit;
    m_host->setObjectName(QStringLiteral("connHost"));
    m_host->setPlaceholderText(QStringLiteral("localhost"));
    serverForm->addRow(QStringLiteral("Host:"), m_host);
    m_port = new QSpinBox;
    m_port->setObjectName(QStringLiteral("connPort"));
    m_port->setRange(1, 65535);
    m_port->setValue(5432);
    serverForm->addRow(QStringLiteral("Port:"), m_port);
    m_dbName = new QLineEdit;
    m_dbName->setObjectName(QStringLiteral("connDatabase"));
    serverForm->addRow(QStringLiteral("Veritabanı:"), m_dbName);
    m_user = new QLineEdit;
    m_user->setObjectName(QStringLiteral("connUser"));
    serverForm->addRow(QStringLiteral("Kullanıcı:"), m_user);
    m_pass = new QLineEdit;
    m_pass->setObjectName(QStringLiteral("connPassword"));
    m_pass->setEchoMode(QLineEdit::Password);
    serverForm->addRow(QStringLiteral("Şifre:"), m_pass);
    layout->addWidget(m_serverGroup);

    m_status = new QLabel;
    m_status->setObjectName(QStringLiteral("connStatus"));
    m_status->setStyleSheet(QStringLiteral("color:#667085;"));
    layout->addWidget(m_status);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto *testBtn = new QPushButton(QStringLiteral("Bağlantıyı Test Et"));
    testBtn->setObjectName(QStringLiteral("connTest"));
    buttons->addButton(testBtn, QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);

    connect(m_driver, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConnectionDialog::onDriverChanged);
    connect(m_browse, &QPushButton::clicked, this, &ConnectionDialog::browseFile);
    connect(testBtn, &QPushButton::clicked, this, &ConnectionDialog::testConnection);
    connect(saveBtn, &QPushButton::clicked, this, &ConnectionDialog::saveCurrent);
    connect(m_saved, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConnectionDialog::loadSaved);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    refreshSavedList();
}

void ConnectionDialog::onDriverChanged(int)
{
    const QString driver = m_driver->currentData().toString();
    const bool sqlite = driver == QStringLiteral("QSQLITE");
    m_sqliteGroup->setVisible(sqlite);
    m_serverGroup->setVisible(!sqlite);
    if (!sqlite) {
        if (driver == QStringLiteral("QPSQL"))
            m_port->setValue(5432);
        else if (driver == QStringLiteral("QMYSQL"))
            m_port->setValue(3306);
    }
}

void ConnectionDialog::browseFile()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("SQLite Dosyası"), QDir::homePath(),
        QStringLiteral("SQLite Database (*.db *.sqlite *.sqlite3);;All Files (*)"),
        nullptr, QFileDialog::DontUseNativeDialog);
    if (!path.isEmpty())
        m_filePath->setText(path);
}

core::ConnectionConfig ConnectionDialog::config() const
{
    core::ConnectionConfig cfg;
    cfg.title = m_title->text().trimmed();
    cfg.driverType = m_driver->currentData().toString();
    cfg.filePath = m_filePath->text().trimmed();
    cfg.host = m_host->text().trimmed();
    cfg.port = m_port->value();
    cfg.databaseName = m_dbName->text().trimmed();
    cfg.userName = m_user->text().trimmed();
    cfg.password = m_pass->text();
    return cfg;
}

void ConnectionDialog::testConnection()
{
    core::DatabaseManager probe;
    QString err;
    if (probe.connect(config(), &err)) {
        probe.closeDatabase();
        m_status->setStyleSheet(QStringLiteral("color:#2e7d32;"));
        m_status->setText(QStringLiteral("✔ Bağlantı başarılı."));
    } else {
        m_status->setStyleSheet(QStringLiteral("color:#b71c1c;"));
        m_status->setText(QStringLiteral("✖ %1").arg(err));
    }
}

void ConnectionDialog::refreshSavedList()
{
    const QString current = m_saved->currentText();
    m_saved->blockSignals(true);
    m_saved->clear();
    QSettings &s = settings();
    s.beginGroup(QStringLiteral("connections"));
    const QStringList names = s.childGroups();
    s.endGroup();
    for (const QString &n : names)
        m_saved->addItem(n);
    m_saved->insertSeparator(m_saved->count());
    m_saved->addItem(QStringLiteral("(yeni…)"));
    const int idx = m_saved->findText(current);
    if (idx >= 0)
        m_saved->setCurrentIndex(idx);
    m_saved->blockSignals(false);
}

void ConnectionDialog::loadSaved(int index)
{
    const QString name = m_saved->itemText(index);
    if (name.isEmpty() || name.startsWith(QStringLiteral("(")))
        return;
    QSettings &s = settings();
    s.beginGroup(QStringLiteral("connections"));
    s.beginGroup(name);
    const int d = m_driver->findData(s.value(QStringLiteral("driver")).toString());
    if (d >= 0)
        m_driver->setCurrentIndex(d);
    m_title->setText(name);
    m_filePath->setText(s.value(QStringLiteral("filePath")).toString());
    m_host->setText(s.value(QStringLiteral("host")).toString());
    m_port->setValue(s.value(QStringLiteral("port"), 5432).toInt());
    m_dbName->setText(s.value(QStringLiteral("database")).toString());
    m_user->setText(s.value(QStringLiteral("user")).toString());
    m_pass->setText(s.value(QStringLiteral("password")).toString());
    s.endGroup();
    s.endGroup();
}

void ConnectionDialog::saveCurrent()
{
    const QString name = m_title->text().trimmed();
    if (name.isEmpty()) {
        m_status->setStyleSheet(QStringLiteral("color:#b71c1c;"));
        m_status->setText(QStringLiteral("Önce bir bağlantı adı girin."));
        return;
    }
    const core::ConnectionConfig cfg = config();
    QSettings &s = settings();
    s.beginGroup(QStringLiteral("connections"));
    s.beginGroup(name);
    s.setValue(QStringLiteral("driver"), cfg.driverType);
    s.setValue(QStringLiteral("filePath"), cfg.filePath);
    s.setValue(QStringLiteral("host"), cfg.host);
    s.setValue(QStringLiteral("port"), cfg.port);
    s.setValue(QStringLiteral("database"), cfg.databaseName);
    s.setValue(QStringLiteral("user"), cfg.userName);
    s.setValue(QStringLiteral("password"), cfg.password);
    s.endGroup();
    s.endGroup();
    refreshSavedList();
    const int idx = m_saved->findText(name);
    if (idx >= 0)
        m_saved->setCurrentIndex(idx);
    m_status->setStyleSheet(QStringLiteral("color:#2e7d32;"));
    m_status->setText(QStringLiteral("✔ Kaydedildi: %1").arg(name));
}

} // namespace ui
