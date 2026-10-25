#pragma once

#include <QDialog>

#include "core/DatabaseManager.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QWidget;

namespace ui {

/// Driver-agnostic "Connect" dialog. SQLite is fully functional; the server
/// fields (host/port/database/user/password) drive QPSQL/QMYSQL/QODBC when
/// those Qt SQL plugins are installed. Saved connections persist via QSettings
/// under "QuickDb/connections".
class ConnectionDialog : public QDialog {
    Q_OBJECT

public:
    explicit ConnectionDialog(const core::ConnectionConfig &initial = {},
                              QWidget *parent = nullptr);

    core::ConnectionConfig config() const;

private slots:
    void onDriverChanged(int index);
    void browseFile();
    void testConnection();
    void loadSaved(int index);
    void saveCurrent();

private:
    void buildUi();
    void refreshSavedList();
    bool available(const QString &driver) const;

    QComboBox *m_saved = nullptr;
    QComboBox *m_driver = nullptr;
    QLineEdit *m_title = nullptr;
    // SQLite
    QLineEdit *m_filePath = nullptr;
    QPushButton *m_browse = nullptr;
    QWidget *m_sqliteGroup = nullptr;
    // Server
    QLineEdit *m_host = nullptr;
    QSpinBox *m_port = nullptr;
    QLineEdit *m_dbName = nullptr;
    QLineEdit *m_user = nullptr;
    QLineEdit *m_pass = nullptr;
    QWidget *m_serverGroup = nullptr;

    QLabel *m_status = nullptr;
};

} // namespace ui
