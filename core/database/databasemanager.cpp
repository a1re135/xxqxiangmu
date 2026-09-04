#include "databasemanager.h"

#include <QDir>
#include <QStandardPaths>
#include <QSqlError>
#include <QSqlQuery>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QResource>

DatabaseManager::DatabaseManager()
{
    QString dataPath =
        QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation
        );

    QDir dir;

    if (!dir.mkpath(dataPath)) {
        qDebug() << "Failed to create application data directory:"
                 << dataPath;
    }

    m_databasePath =
        QDir(dataPath).filePath("charge_platform.db");
}

bool DatabaseManager::openDatabase()
{
    if (QSqlDatabase::contains("ncs_connection")) {
        m_database =
            QSqlDatabase::database("ncs_connection");
    } else {
        m_database =
            QSqlDatabase::addDatabase(
                "QSQLITE",
                "ncs_connection"
            );
    }

    m_database.setDatabaseName(m_databasePath);

    if (!m_database.open()) {
        qDebug() << "Failed to open database:"
                 << m_database.lastError().text();

        return false;
    }

    qDebug() << "Database opened successfully:";
    qDebug() << m_databasePath;

    QSqlQuery query(m_database);

    if (!query.exec("PRAGMA foreign_keys = ON;")) {
        qDebug()
            << "Failed to enable foreign keys:"
            << query.lastError().text();

        return false;
    }

    return true;
}

bool DatabaseManager::createTables()
{
    QFile schemaFile(":/db/schema.sql");

    if (!schemaFile.open(
            QIODevice::ReadOnly |
            QIODevice::Text)) {

        qDebug()
            << "Failed to open schema.sql:"
            << schemaFile.errorString();

        return false;
    }

    QTextStream stream(&schemaFile);
    QString sql = stream.readAll();

    schemaFile.close();

    QStringList statements =
        sql.split(
            ';',
            Qt::SkipEmptyParts
        );

    if (!m_database.transaction()) {
        qDebug()
            << "Failed to start database transaction:"
            << m_database.lastError().text();

        return false;
    }

    QSqlQuery query(m_database);

    for (QString statement : statements) {

        statement = statement.trimmed();

        if (statement.isEmpty()) {
            continue;
        }

        if (!query.exec(statement)) {

            qDebug()
                << "Failed SQL statement:"
                << statement;

            qDebug()
                << "SQLite error:"
                << query.lastError().text();

            m_database.rollback();

            return false;
        }
    }

    if (!query.exec(
            "INSERT INTO schema_version(version) "
            "SELECT 1 "
            "WHERE NOT EXISTS "
            "(SELECT 1 FROM schema_version);")) {

        qDebug()
            << "Failed to initialize schema version:"
            << query.lastError().text();

        m_database.rollback();

        return false;
    }

    if (!m_database.commit()) {

        qDebug()
            << "Failed to commit schema creation:"
            << m_database.lastError().text();

        m_database.rollback();

        return false;
    }

    qDebug() << "Database tables created successfully.";

    return true;
}

bool DatabaseManager::initialize()
{
    if (!openDatabase()) {
        qDebug()
            << "Database initialization failed while opening database.";

        return false;
    }

    if (!createTables()) {
        qDebug()
            << "Database initialization failed while creating tables.";

        return false;
    }

    qDebug()
        << "Database initialization completed successfully.";

    return true;
}
