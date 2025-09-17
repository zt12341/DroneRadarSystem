#include <QApplication>
#include "RadarConfig.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    RadarConfig config;
    config.show();
    
    return app.exec();
}
