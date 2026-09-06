#include "mainwindow.h"

#include "database/databasemanager.h"
#include "service/station_service.h"

#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    DatabaseManager databaseManager;

    if (!databaseManager.initialize()) {
        qDebug() << "Database initialization failed.";
        return -1;
    }

    core::StationService stationService(&databaseManager);

    MainWindow w(&stationService);
    w.show();

    return a.exec();
}
