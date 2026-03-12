#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Disable icons on standard dialog buttons (OK, Cancel, etc.)
    app.setAttribute(Qt::AA_DontShowIconsInMenus, false);
    app.setStyleSheet("QDialogButtonBox QPushButton { icon-size: 0px; }"
                       "QMessageBox QPushButton { icon-size: 0px; }");

    MainWindow window;
    window.show();

    return app.exec();
}
