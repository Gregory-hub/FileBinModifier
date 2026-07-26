#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("FileBinModifier"));
    QApplication::setOrganizationName(QStringLiteral("FileBinModifier"));

    MainWindow window;
    window.show();

    return QApplication::exec();
}
