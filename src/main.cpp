#include <QApplication>

#include "ui/MainWindow.h"
#include "ui/Theme.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("QuickDb"));
    app.setOrganizationName(QStringLiteral("QuickDb"));

    ui::Theme::apply(&app, ui::Theme::Mode::Light);

    ui::MainWindow window;
    window.show();
    window.raise();
    window.activateWindow(); // force foreground so modals/sheets attach properly

    return app.exec();
}
