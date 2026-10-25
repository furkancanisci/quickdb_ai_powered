#include "ui/Theme.h"

#include <QApplication>
#include <QPalette>

namespace ui {

namespace {

Theme::Mode g_mode = Theme::Mode::Light;

// Shared structural styling. Colors are driven by the palette where possible,
// with a few explicit accents for premium feel (buttons, tabs, badges).
const char *kCommonQss = R"(
QWidget { font-size: 13px; }

QMainWindow::separator { background: palette(mid); width: 3px; height: 3px; }
QSplitter::handle { background: palette(mid); }
QSplitter::handle:horizontal { width: 3px; }
QSplitter::handle:vertical { height: 3px; }

/* Group boxes as soft cards */
QGroupBox {
    border: 1px solid palette(mid);
    border-radius: 10px;
    margin-top: 14px;
    padding: 10px 8px 8px 8px;
    font-weight: 600;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 12px;
    padding: 0 6px;
}

/* Menu + toolbar: the single unified command bar */
QMenuBar {
    padding: 4px 8px;
    border-bottom: 1px solid palette(mid);
    background: palette(window);
}
QMenuBar::item {
    padding: 6px 12px;
    margin: 0 2px;
    background: transparent;
    border-radius: 6px;
    font-weight: 500;
}
QMenuBar::item:selected { background: palette(highlight); color: palette(highlighted-text); }
QMenuBar::item:pressed:!selected { background: palette(midlight); }
QMenu { border: 1px solid palette(mid); border-radius: 8px; padding: 6px; }
QMenu::item { padding: 6px 22px; border-radius: 6px; }
QMenu::item:selected { background: palette(highlight); color: palette(highlighted-text); }
QMenu::separator { height: 1px; background: palette(mid); margin: 6px 10px; }
QToolBar { border: none; padding: 4px; spacing: 4px; }
QToolButton { padding: 6px 10px; border-radius: 6px; }
QToolButton:hover { background: palette(midlight); }
QToolButton:pressed { background: palette(mid); }

/* Push buttons */
QPushButton {
    padding: 6px 14px;
    border-radius: 6px;
    border: 1px solid palette(mid);
    background: palette(button);
}
QPushButton:hover { background: palette(midlight); }
QPushButton:pressed { background: palette(mid); }
QPushButton:disabled { color: palette(mid); }

/* Inputs */
QPlainTextEdit, QLineEdit, QTextEdit {
    border: 1px solid palette(mid);
    border-radius: 6px;
    padding: 4px;
    selection-background-color: palette(highlight);
    selection-color: palette(highlighted-text);
}
/* SQL editor: explicit high-contrast text so nothing ever renders washed out
   (beats the wildcard color rules above). */
#queryEditor {
    background: palette(base);
    color: palette(text);
    font-size: 13px;
}

/* Tabs */
QTabWidget::pane { border: 1px solid palette(mid); border-radius: 8px; top: -1px; }
QTabBar::tab {
    padding: 7px 14px;
    border: 1px solid palette(mid);
    border-bottom: none;
    border-top-left-radius: 8px;
    border-top-right-radius: 8px;
    margin-right: 2px;
    background: palette(midlight);
}
QTabBar::tab:selected { background: palette(base); }
QTabBar::tab:!selected { margin-top: 2px; }

/* Item views: tree + table */
QTreeView, QTableView {
    border: 1px solid palette(mid);
    border-radius: 8px;
    alternate-background-color: palette(alternate-base);
    selection-background-color: palette(highlight);
    selection-color: palette(highlighted-text);
    gridline-color: palette(mid);
}
QTreeView::item, QTableView::item { padding: 3px; }
QHeaderView::section {
    background: palette(midlight);
    padding: 6px 8px;
    border: none;
    border-right: 1px solid palette(mid);
    border-bottom: 1px solid palette(mid);
    font-weight: 600;
}
QTableView QTableCornerButton::section { background: palette(midlight); border: none; }

/* Status bar */
QStatusBar { padding: 2px; }
QStatusBar::item { border: none; }

/* Scrollbars: slim, modern */
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: palette(mid); border-radius: 5px; min-height: 24px; }
QScrollBar::handle:vertical:hover { background: palette(dark); }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: palette(mid); border-radius: 5px; min-width: 24px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

/* ComboBox */
QComboBox {
    padding: 4px 10px;
    border: 1px solid palette(mid);
    border-radius: 6px;
    background: palette(base);
}
QComboBox::drop-down { border: none; width: 18px; }
QComboBox QAbstractItemView {
    border: 1px solid palette(mid);
    border-radius: 6px;
    selection-background-color: palette(highlight);
    selection-color: palette(highlighted-text);
}
)";

// Light-only touches (accent colors on top of the palette-driven common QSS).
const char *kLightQss = R"(
* { color: #1f2328; }
QWidget { background: #f4f6f9; }
QGroupBox { background: #ffffff; }
#queryEditor { background: #ffffff; color: #101828; }
#executeButton {
    background-color: #2563eb; color: #ffffff; font-weight: 700;
    border: none; border-radius: 6px; padding: 7px 12px;
}
#executeButton:hover { background-color: #1d4ed8; }
#aiSendButton {
    background-color: #2563eb; color: #ffffff; font-weight: 700; border: none; border-radius: 6px;
}
#aiSendButton:hover { background-color: #1d4ed8; }
#aiChat { background: #f2f4f7; }
#aiInput { background: #ffffff; }
#aiSummary, #aiReport { background: #f4f6ff; }
)";

const char *kDarkQss = R"(
* { color: #e6e6ea; }
QWidget { background: #1e1f26; }
QGroupBox { background: #262832; border-color: #3a3d4a; }
#queryEditor { background: #23242c; color: #e6e6ea; }
QTabBar::tab { background: #2a2c36; }
QTabBar::tab:selected { background: #262832; }
QPlainTextEdit, QLineEdit, QTextEdit, QComboBox { background: #23242c; border-color: #3a3d4a; }
QTreeView, QTableView { background: #23242c; border-color: #3a3d4a; gridline-color: #34363f; }
QHeaderView::section { background: #2a2c36; border-color: #3a3d4a; }
QMenu { background: #262832; }
#executeButton {
    background-color: #3b82f6; color: #0b0f19; font-weight: 700;
    border: none; border-radius: 6px; padding: 7px 12px;
}
#executeButton:hover { background-color: #60a5fa; }
#aiSendButton {
    background-color: #3b82f6; color: #0b0f19; font-weight: 700; border: none; border-radius: 6px;
}
#aiChat { background: #23242c; }
#aiInput { background: #1b1c22; }
#aiSummary, #aiReport { background: #262832; }
)";

void applyLightPalette(QApplication *app)
{
    QPalette p;
    p.setColor(QPalette::Window, QColor(0xf4, 0xf6, 0xf9));
    p.setColor(QPalette::Base, QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::AlternateBase, QColor(0xf7, 0xf9, 0xfc));
    p.setColor(QPalette::Text, QColor(0x1f, 0x23, 0x28));
    p.setColor(QPalette::Button, QColor(0xf0, 0xf2, 0xf6));
    p.setColor(QPalette::ButtonText, QColor(0x1f, 0x23, 0x28));
    p.setColor(QPalette::WindowText, QColor(0x1f, 0x23, 0x28));
    p.setColor(QPalette::Mid, QColor(0xd6, 0xdb, 0xe3));
    p.setColor(QPalette::Midlight, QColor(0xea, 0xed, 0xf2));
    p.setColor(QPalette::Dark, QColor(0xb6, 0xbc, 0xc6));
    p.setColor(QPalette::Light, QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::Highlight, QColor(0x25, 0x63, 0xeb));
    p.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::ToolTipBase, QColor(0xff, 0xfb, 0xe1));
    p.setColor(QPalette::ToolTipText, QColor(0x1f, 0x23, 0x28));
    app->setPalette(p);
}

void applyDarkPalette(QApplication *app)
{
    QPalette p;
    const QColor window(0x1e, 0x1f, 0x26);
    const QColor base(0x23, 0x24, 0x2c);
    const QColor text(0xe6, 0xe6, 0xea);
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, QColor(0x2a, 0x2c, 0x36));
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, QColor(0x2a, 0x2c, 0x36));
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Mid, QColor(0x3a, 0x3d, 0x4a));
    p.setColor(QPalette::Midlight, QColor(0x2a, 0x2c, 0x36));
    p.setColor(QPalette::Dark, QColor(0x12, 0x13, 0x18));
    p.setColor(QPalette::Light, QColor(0x4a, 0x4d, 0x5a));
    p.setColor(QPalette::Highlight, QColor(0x3b, 0x82, 0xf6));
    p.setColor(QPalette::HighlightedText, QColor(0x0b, 0x0f, 0x19));
    p.setColor(QPalette::ToolTipBase, QColor(0x2a, 0x2c, 0x36));
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::PlaceholderText, QColor(0x9a, 0x9d, 0xaa));
    app->setPalette(p);
}

} // namespace

void Theme::apply(QApplication *app, Mode mode)
{
    if (!app)
        return;
    g_mode = mode;
    app->setStyle(QStringLiteral("Fusion")); // consistent cross-platform base
    if (mode == Mode::Dark) {
        applyDarkPalette(app);
        app->setStyleSheet(QString::fromUtf8(kCommonQss) + QString::fromUtf8(kDarkQss));
    } else {
        applyLightPalette(app);
        app->setStyleSheet(QString::fromUtf8(kCommonQss) + QString::fromUtf8(kLightQss));
    }
}

Theme::Mode Theme::current()
{
    return g_mode;
}

void Theme::toggle(QApplication *app)
{
    apply(app, g_mode == Mode::Light ? Mode::Dark : Mode::Light);
}

} // namespace ui
