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
    QString basePath =
        QStandardPaths::writableLocation(
            QStandardPaths::GenericDataLocation
        );

    QString dataPath =
        QDir(basePath).filePath(
            "NCS_Charging_Platform"
        );

    QDir dir;

    if (!dir.mkpath(dataPath)) {
        qDebug() << "Failed to create database directory:"
                 << dataPath;
    }

    m_databasePath =
        QDir(dataPath).filePath(
            "charge_platform.db"
        );

    qDebug() << "Database path:" << m_databasePath;
}

bool DatabaseManager::openDatabase()
{
    if (QSqlDatabase::contains("ncs_connection")) {

        m_database =
            QSqlDatabase::database(
                "ncs_connection"
            );

    } else {

        m_database =
            QSqlDatabase::addDatabase(
                "QSQLITE",
                "ncs_connection"
            );
    }

    m_database.setDatabaseName(
        m_databasePath
    );

    if (!m_database.open()) {

        qDebug() << "Failed to open database:"
                 << m_database.lastError().text();

        return false;
    }

    qDebug() << "Database opened successfully:"
             << m_databasePath;

    if (!configureDatabase()) {
        qDebug()
            << "Failed to configure database.";
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

    QList<int> chargerIds;

    QSqlQuery chargerIdQuery(m_database);

    if (!chargerIdQuery.exec("SELECT id FROM charger ORDER BY id")) {
        qDebug() << "Failed to get charger IDs:"
                 << chargerIdQuery.lastError().text();

        m_database.rollback();
        return false;
    }

    while (chargerIdQuery.next()) {
        chargerIds.append(
            chargerIdQuery.value(0).toInt()
        );
    }

    qDebug() << "Charger IDs loaded:" << chargerIds.size();

    QList<int> userIds;

    QSqlQuery userQuery(m_database);

    userQuery.prepare(
        "INSERT INTO user "
        "(phone, nickname, avatar_path, balance, register_time, status) "
        "VALUES "
        "(:phone, :nickname, :avatar_path, :balance, :register_time, :status)"
    );

    for (int i = 1; i <= 5; ++i) {

        QString phone =
            QString("138000000%1")
                .arg(i);

        QString nickname =
            QString("用户%1")
                .arg(i, 4, 10, QChar('0'));

        userQuery.bindValue(":phone", phone);
        userQuery.bindValue(":nickname", nickname);
        userQuery.bindValue(":avatar_path", "");
        userQuery.bindValue(":balance", 300.0);
        userQuery.bindValue(
            ":register_time",
            QDateTime::currentDateTime()
                .addDays(-30)
                .toString("yyyy-MM-dd HH:mm:ss")
        );
        userQuery.bindValue(":status", 1);

        if (!userQuery.exec()) {

            qDebug() << "Failed to insert user:"
                     << userQuery.lastError().text();

            m_database.rollback();
            return false;
        }

        userIds.append(
            userQuery.lastInsertId().toInt()
        );
    }

    qDebug() << "Demo users inserted:" << userIds;

    QSqlQuery orderQuery(m_database);

    orderQuery.prepare(
        "INSERT INTO charging_order "
        "(user_id, charger_id, start_time, end_time, "
        "energy, amount, status) "
        "VALUES "
        "(:user_id, :charger_id, :start_time, :end_time, "
        ":energy, :amount, :status)"
    );

    int historicalOrderCount = 0;

    for (int daysAgo = 30; daysAgo >= 1; --daysAgo) {

        for (int orderIndex = 0; orderIndex < 3; ++orderIndex) {

            int userId =
                userIds[
                    QRandomGenerator::global()->bounded(
                        userIds.size()
                    )
                ];

            int chargerId =
                chargerIds[
                    QRandomGenerator::global()->bounded(
                        chargerIds.size()
                    )
                ];

            int hour =
                QRandomGenerator::global()->bounded(7, 22);

            int minute =
                QRandomGenerator::global()->bounded(0, 60);

            QDateTime startTime =
                QDateTime::currentDateTime()
                    .addDays(-daysAgo);

            startTime.setTime(
                QTime(hour, minute, 0)
            );

            int durationMinutes =
                QRandomGenerator::global()->bounded(30, 121);

            QDateTime endTime =
                startTime.addSecs(
                    durationMinutes * 60
                );

            double energy =
                QRandomGenerator::global()->bounded(
                    1000, 6001
                ) / 100.0;

            QSqlQuery priceQuery(m_database);

            priceQuery.prepare(
                "SELECT station.price "
                "FROM charger "
                "JOIN station "
                "ON charger.station_id = station.id "
                "WHERE charger.id = :charger_id"
            );

            priceQuery.bindValue(
                ":charger_id",
                chargerId
            );

            if (!priceQuery.exec() ||
                !priceQuery.next()) {

                qDebug()
                    << "Failed to get station price:"
                    << priceQuery.lastError().text();

                m_database.rollback();
                return false;
            }

            double price =
                priceQuery.value(0).toDouble();

            double amount =
                qRound64(energy * price * 100.0)
                / 100.0;

            orderQuery.bindValue(
                ":user_id",
                userId
            );

            orderQuery.bindValue(
                ":charger_id",
                chargerId
            );

            orderQuery.bindValue(
                ":start_time",
                startTime.toString(
                    "yyyy-MM-dd HH:mm:ss"
                )
            );

            orderQuery.bindValue(
                ":end_time",
                endTime.toString(
                    "yyyy-MM-dd HH:mm:ss"
                )
            );

            orderQuery.bindValue(
                ":energy",
                energy
            );

            orderQuery.bindValue(
                ":amount",
                amount
            );

            // 2 = completed
            orderQuery.bindValue(
                ":status",
                2
            );

            if (!orderQuery.exec()) {

                qDebug()
                    << "Failed to insert historical order:"
                    << orderQuery.lastError().text();

                m_database.rollback();
                return false;
            }

            ++historicalOrderCount;
        }
    }

    qDebug() << "Historical orders inserted:" << historicalOrderCount;

    if (!m_database.commit()) {

        qDebug() << "Failed to commit seed data:"
                 << m_database.lastError().text();

        m_database.rollback();
        return false;
    }

    qDebug() << "Seed data inserted successfully.";

    return true;
}

bool DatabaseManager::configureDatabase()
{
    QSqlQuery query(m_database);

    if (!query.exec("PRAGMA foreign_keys = ON;")) {
        qDebug() << "Failed to enable foreign keys:"
                 << query.lastError().text();
        return false;
    }

    if (!query.exec("PRAGMA foreign_keys;")) {
        qDebug() << "Failed to check foreign-key status:"
                 << query.lastError().text();
        return false;
    }

    if (!query.next() || query.value(0).toInt() != 1) {
        qDebug() << "Foreign-key checking is NOT enabled.";
        return false;
    }

    qDebug() << "Foreign-key checking enabled.";

    if (!query.exec("PRAGMA journal_mode = WAL;")) {
        qDebug() << "Failed to enable WAL:"
                 << query.lastError().text();
        return false;
    }

    if (!query.next()) {
        qDebug() << "Could not read WAL mode result.";
        return false;
    }

    QString journalMode =
        query.value(0).toString().toLower();

    qDebug() << "SQLite journal mode:"
             << journalMode;

    if (journalMode != "wal") {
        qDebug() << "WAL mode was not enabled.";
        return false;
    }

    return true;
}

bool DatabaseManager::beginTransaction()
{
    if (!m_database.transaction()) {

        qDebug() << "Failed to begin transaction:"
                 << m_database.lastError().text();

        return false;
    }

    return true;
}

bool DatabaseManager::commitTransaction()
{
    if (!m_database.commit()) {

        qDebug() << "Failed to commit transaction:"
                 << m_database.lastError().text();

        return false;
    }

    return true;
}

bool DatabaseManager::rollbackTransaction()
{
    if (!m_database.rollback()) {

        qDebug() << "Failed to rollback transaction:"
                 << m_database.lastError().text();

        return false;
    }

    return true;
}

QSqlDatabase DatabaseManager::connection() const
{
    return m_database;
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
