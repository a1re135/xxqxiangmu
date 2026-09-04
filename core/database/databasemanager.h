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

private:
    QSqlDatabase m_database;
    QString m_databasePath;
};

#endif // DATABASEMANAGER_H
