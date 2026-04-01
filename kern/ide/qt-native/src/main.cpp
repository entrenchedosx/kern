#include <QApplication>
#include "MainWindow.hpp"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Kern IDE");
    app.setOrganizationName("Kern");

    MainWindow window;
    window.show();
    return app.exec();
}
