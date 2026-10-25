#pragma once

#include <QDialog>

#include "core/io/Exporters.h"

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

namespace ui {

/// Chooses an export format, live-previews the serialized document and writes
/// it to a target file. Purely presentational — the actual serialization lives
/// in core::io::Exporters so it can be unit-tested headlessly.
class ExportDialog : public QDialog {
    Q_OBJECT

public:
    explicit ExportDialog(const core::io::ExportInput &data, QWidget *parent = nullptr);

    /// Current format ("csv"|"json"|"xml"|"sql").
    QString format() const;
    /// The document rendered with the currently selected format.
    QString rendered() const;
    QString filePath() const;

    /// Renders and writes the document to `path` (defaults to the file field).
    /// Returns an empty string on success, otherwise the error text.
    QString writeTo(const QString &path = QString());

private slots:
    void refreshPreview();
    void browse();

private:
    core::io::ExportInput m_data;
    QComboBox *m_format = nullptr;
    QPlainTextEdit *m_preview = nullptr;
    QLineEdit *m_file = nullptr;
    QPushButton *m_browse = nullptr;
};

} // namespace ui
