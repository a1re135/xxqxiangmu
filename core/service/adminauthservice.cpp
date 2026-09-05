#include "adminauthservice.h"

#include <QCryptographicHash>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

bool AdminAuthService::verifyPassword(
    const QString &password,
    const QString &storedHash,
    const QString &salt) const
{
    // Matches DatabaseManager::insertSeedData():
    // SHA-256(salt + password)
    const QByteArray hash = QCryptographicHash::hash(
        (salt + password).toUtf8(),
        QCryptographicHash::Sha256
    );

    return QString::fromLatin1(hash.toHex()).compare(
        storedHash,
        Qt::CaseInsensitive
    ) == 0;
}

bool AdminAuthService::authenticate(
    const QString &username,
    const QString &password,
    AdminInfo &adminInfo,
    QString &errorMessage) const
{
    errorMessage.clear();
    adminInfo = AdminInfo{};

    const QSqlDatabase db =
        QSqlDatabase::database("ncs_connection");

    if (!db.isValid() || !db.isOpen()) {
        errorMessage = QStringLiteral("数据库未打开，无法验证管理员身份");
        return false;
    }

    QSqlQuery query(db);

    query.prepare(
        "SELECT id, username, password_hash, salt "
        "FROM admin "
        "WHERE username = :username "
        "LIMIT 1"
    );

    query.bindValue(":username", username);

    if (!query.exec()) {
        errorMessage =
            QStringLiteral("管理员验证失败：") +
            query.lastError().text();
        return false;
    }

    if (!query.next()) {
        // Important: caller should expose only the generic
        // authentication error to avoid username enumeration.
        errorMessage = QStringLiteral("账号或密码错误");
        return false;
    }

    const int id = query.value("id").toInt();
    const QString dbUsername = query.value("username").toString();
    const QString storedHash = query.value("password_hash").toString();
    const QString salt = query.value("salt").toString();

    if (!verifyPassword(password, storedHash, salt)) {
        errorMessage = QStringLiteral("账号或密码错误");
        return false;
    }

    adminInfo.id = id;
    adminInfo.username = dbUsername;

    return true;
}
