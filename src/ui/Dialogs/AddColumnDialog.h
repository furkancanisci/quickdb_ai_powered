#pragma once

#include <QDialog>
#include "core/DatabaseManager.h"

class QComboBox;
class QLineEdit;

namespace ui {

/// ALTER TABLE ... ADD COLUMN helper dialog.
class AddColumnDialog : public QDialog {
    Q_OBJECT

public:
    explicit AddColumnDialog(const QStringList &tables, bool isPostgres = false,
                             QWidget *parent = nullptr);

    QString targetTable() const;
    core::ColumnSpec columnSpec() const;
    void setCurrentTableIndex(int index);

private:
    QComboBox *m_table = nullptr;
    QLineEdit *m_name = nullptr;
    QComboBox *m_type = nullptr;
    QLineEdit *m_default = nullptr;
};

} // namespace ui
