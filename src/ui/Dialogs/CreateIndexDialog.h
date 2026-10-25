#pragma once

#include <QDialog>
#include <QStringList>

class QCheckBox;
class QLineEdit;
class QListWidget;

namespace ui {

/// Dialog for CREATE INDEX: pick one or more columns of a fixed table, choose
/// UNIQUE, and optionally override the generated index name.
class CreateIndexDialog : public QDialog {
    Q_OBJECT

public:
    explicit CreateIndexDialog(const QString &table, const QStringList &columns,
                               QWidget *parent = nullptr);

    QString indexName() const;
    QStringList selectedColumns() const;
    bool isUnique() const;

private:
    QLineEdit *m_name = nullptr;
    QListWidget *m_columns = nullptr;
    QCheckBox *m_unique = nullptr;
};

} // namespace ui
