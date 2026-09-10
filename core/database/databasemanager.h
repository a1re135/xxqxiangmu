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

    QSqlDatabase connection() const;

    QString lastErrorMessage() const;
    bool isCorrupted() const;

private:
    QSqlDatabase m_database;
    QString m_databasePath;

    QString m_lastErrorMessage;
    bool m_corrupted = false;

    bool configureDatabase();
    bool checkDatabaseIntegrity();

    bool upgradeReservationSchema();
    bool upgradeSettlementSchema();
};

#endif // DATABASEMANAGER_H
