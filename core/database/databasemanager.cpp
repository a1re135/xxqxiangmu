#include "databasemanager.h"

#include <QDir>
#include <QStandardPaths>
#include <QSqlError>
#include <QSqlQuery>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QResource>
#include <QCryptographicHash>
#include <QDateTime>
#include <QRandomGenerator>
#include <QUuid>

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

bool DatabaseManager::insertSeedData()
{
    QSqlQuery checkQuery(m_database);

    if (!checkQuery.exec("SELECT COUNT(*) FROM station")) {
        qDebug() << "Failed to check seed data:"
                 << checkQuery.lastError().text();
        return false;
    }

    if (checkQuery.next() && checkQuery.value(0).toInt() > 0) {
        qDebug() << "Seed data already exists. Skipping.";
        return true;
    }

    if (!m_database.transaction()) {
        qDebug() << "Failed to start seed transaction:"
                 << m_database.lastError().text();
        return false;
    }

    QString username = "admin";
    QString password = "123456";

    // Generate a random salt
    QString salt =
        QUuid::createUuid().toString(QUuid::WithoutBraces);

    // password hash = SHA256(salt + password)
    QByteArray hash =
        QCryptographicHash::hash(
            (salt + password).toUtf8(),
            QCryptographicHash::Sha256
        );

    QString passwordHash =
        QString(hash.toHex());

    QSqlQuery adminQuery(m_database);

    adminQuery.prepare(
        "INSERT INTO admin "
        "(username, password_hash, salt) "
        "VALUES "
        "(:username, :password_hash, :salt)"
    );

    adminQuery.bindValue(":username", username);
    adminQuery.bindValue(":password_hash", passwordHash);
    adminQuery.bindValue(":salt", salt);

    if (!adminQuery.exec()) {
        qDebug() << "Failed to insert default administrator:"
                 << adminQuery.lastError().text();

        m_database.rollback();
        return false;
    }

    qDebug() << "Default administrator inserted successfully.";

    struct StationSeed
    {
        QString name;
        QString address;
        double longitude;
        double latitude;
        double price;
    };

    QList<StationSeed> stations = {
        {
            "Central Charging Station",
            "Beijing Central Area",
            116.4074,
            39.9042,
            1.50
        },
        {
            "Haidian Charging Station",
            "Haidian District",
            116.2981,
            39.9593,
            1.60
        },
        {
            "Chaoyang Charging Station",
            "Chaoyang District",
            116.4435,
            39.9219,
            1.55
        },
        {
            "Fengtai Charging Station",
            "Fengtai District",
            116.2869,
            39.8584,
            1.45
        },
        {
            "Shijingshan Charging Station",
            "Shijingshan District",
            116.2229,
            39.9062,
            1.50
        }
    };


    QSqlQuery stationQuery(m_database);

    stationQuery.prepare(
        "INSERT INTO station "
        "(name, address, longitude, latitude, price) "
        "VALUES "
        "(:name, :address, :longitude, :latitude, :price)"
    );

    QList<int> stationIds;

    for (const StationSeed &station : stations) {

        stationQuery.bindValue(":name", station.name);
        stationQuery.bindValue(":address", station.address);
        stationQuery.bindValue(":longitude", station.longitude);
        stationQuery.bindValue(":latitude", station.latitude);
        stationQuery.bindValue(":price", station.price);

        if (!stationQuery.exec()) {

            qDebug() << "Failed to insert station:"
                     << stationQuery.lastError().text();

            m_database.rollback();
            return false;
        }

        stationIds.append(
            stationQuery.lastInsertId().toInt()
        );
    }

    qDebug() << "5 charging stations inserted successfully.";
    qDebug() << "Station IDs:" << stationIds;

    QSqlQuery chargerQuery(m_database);

    chargerQuery.prepare(
        "INSERT INTO charger "
        "(station_id, charger_no, type, power, status, "
        "total_count, total_minutes) "
        "VALUES "
        "(:station_id, :charger_no, :type, :power, :status, "
        ":total_count, :total_minutes)"
    );

    for (int stationIndex = 0;
         stationIndex < stationIds.size();
         ++stationIndex) {

        int stationId = stationIds[stationIndex];

        // Create 8 chargers for each station
        for (int i = 1; i <= 8; ++i) {

            QString chargerNo =
                QString("ST%1-%2")
                    .arg(stationIndex + 1)
                    .arg(i, 2, 10, QChar('0'));

            int type;
            double power;
            int status;

            // Chargers 1-4: fast charging
            if (i <= 4) {
                type = 1;
                power = 120.0;
            }
            // Chargers 5-8: slow charging
            else {
                type = 0;
                power = 7.0;
            }

            // Charger 8 is faulty
            if (i == 8) {
                status = 2;
            } else {
                status = 0;
            }

            chargerQuery.bindValue(":station_id", stationId);
            chargerQuery.bindValue(":charger_no", chargerNo);
            chargerQuery.bindValue(":type", type);
            chargerQuery.bindValue(":power", power);
            chargerQuery.bindValue(":status", status);
            chargerQuery.bindValue(":total_count", 0);
            chargerQuery.bindValue(":total_minutes", 0);

            if (!chargerQuery.exec()) {
                qDebug() << "Failed to insert charger:"
                         << chargerQuery.lastError().text();

                m_database.rollback();
                return false;
            }
        }
    }

    qDebug() << "Chargers inserted successfully.";

    if (!m_database.commit()) {

        qDebug() << "Failed to commit seed data:"
                 << m_database.lastError().text();

        m_database.rollback();
        return false;
    }

    qDebug() << "Seed data inserted successfully.";

    return true;
}

bool DatabaseManager::initialize()
{
    if (!openDatabase()) {
        return false;
    }

    if (!createTables()) {
        return false;
    }

    if (!insertSeedData()) {
        return false;
    }

    qDebug() << "Database initialization completed successfully.";

    return true;
}
