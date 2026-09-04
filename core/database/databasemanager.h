#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QSqlDatabase>
#include <QString>

class DatabaseManager
{
public:
    DatabaseManager();

    bool initialize();
    bool openDatabase();
    bool createTables();
    bool insertSeedData();

    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

private:
    QSqlDatabase m_database;
    QString m_databasePath;
    bool configureDatabase();
};

#endif // DATABASEMANAGER_H
