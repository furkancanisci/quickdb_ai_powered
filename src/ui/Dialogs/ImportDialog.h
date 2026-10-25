#pragma once

#include <QDialog>

#include "core/DatabaseManager.h"
#include "core/io/Importers.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace ui {

/// Guided file import: pick a CSV/JSON file, tune the delimiter/header options,
/// preview the parsed rows, then create (or reuse) a target table and load the
/// data through core::DatabaseManager. Returns error text to the caller so the
/// MainWindow can surface it in the status bar.
class ImportDialog : public QDialog {
    Q_OBJECT

public:
    explicit ImportDialog(core::DatabaseManager *db, QWidget *parent = nullptr);

    /// Parses the current file into the preview grid. Empty string on success.
    QString loadPreview();
    /// Creates the target table (if needed) and inserts every previewed row
    /// inside a transaction. Empty string on success, otherwise the error text.
    QString runImport();

    const core::io::ImportedTable &parsed() const { return m_parsed; }
    QString targetTable() const;

private slots:
    void browse();
    void onLoadClicked();
    void onImportClicked();

private:
    QChar currentDelimiter() const;
    void fillPreview();

    core::DatabaseManager *m_db = nullptr;
    core::io::ImportedTable m_parsed;

    QLineEdit *m_file = nullptr;
    QPushButton *m_browse = nullptr;
    QComboBox *m_delim = nullptr;
    QCheckBox *m_header = nullptr;
    QLineEdit *m_table = nullptr;
    QTableWidget *m_preview = nullptr;
    QLabel *m_status = nullptr;
};

} // namespace ui
