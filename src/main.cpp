#include <QApplication>
#include <QIcon>
#include <QSettings>
#include <QStyleFactory>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Apply saved widget style before creating any widgets
    QSettings s("vibe-coder", "vibe-coder");
    QString widgetStyle = s.value("global/widgetStyle", "Auto").toString();
    if (widgetStyle != "Auto") {
        QStyle *style = QStyleFactory::create(widgetStyle);
        if (style) app.setStyle(style);
    }

    // Disable icons on standard dialog buttons (OK, Cancel, etc.)
    app.setAttribute(Qt::AA_DontShowIconsInMenus, false);
    app.setStyleSheet("QDialogButtonBox QPushButton { icon-size: 0px; }"
                       "QMessageBox QPushButton { icon-size: 0px; }");

    app.setWindowIcon(QIcon(":/vibe-coder.png"));

    MainWindow window;
    window.show();

    return app.exec();
}
