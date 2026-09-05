#include "mainwindow.h"

#include <QApplication>
#include <QDebug>

#include "database/databasemanager.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    DatabaseManager databaseManager;

    if (!databaseManager.initialize()) {
        qDebug() << "Unable to initialize database.";
        return -1;
    }

    MainWindow w;
    w.show();

    return a.exec();
}
