#pragma once

#include <QDialog>

class QLineEdit;
class QPlainTextEdit;

namespace ui {

/// Minimal dialog for CREATE VIEW: a name plus the SELECT body.
class CreateViewDialog : public QDialog {
    Q_OBJECT

public:
    explicit CreateViewDialog(QWidget *parent = nullptr);

    QString viewName() const;
    QString selectSql() const;

private:
    QLineEdit *m_name = nullptr;
    QPlainTextEdit *m_select = nullptr;
};

} // namespace ui
