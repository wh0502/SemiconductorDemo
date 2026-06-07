#include "SemiconductorDemo.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    SemiconductorDemo window;
    window.setWindowTitle(QString::fromLocal8Bit("半导体设备模拟上位机 Demo"));
    window.resize(1200, 800);
    window.show();
    return app.exec();
}
