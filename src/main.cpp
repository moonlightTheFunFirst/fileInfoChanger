#include "MainWindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app-icon.ico")));

    MainWindow window;
    window.setWindowTitle(QStringLiteral("fileInfoChanger"));
    window.setWindowIcon(QIcon(QStringLiteral(":/icons/app-icon.ico")));
    window.show();

    return app.exec();
}
