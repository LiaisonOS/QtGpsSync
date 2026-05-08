#include <QApplication>
#include "MainWindow.h"
#include "NmeaParser.h"

int main(int argc, char *argv[])
{
    qRegisterMetaType<RmcFix>("RmcFix");

    QApplication app(argc, argv);
    app.setApplicationName("QtGpsSync");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("LiaisonOS");

    MainWindow w;
    w.show();

    return app.exec();
}
