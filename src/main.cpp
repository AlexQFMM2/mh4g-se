#include "mainwindow.hpp"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName("MH4G Save Editor");
    QApplication::setOrganizationName("mh4g-se");
    MainWindow window;
    window.show();
    return application.exec();
}
