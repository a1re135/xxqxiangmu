#include "userservice.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QDateTime>

UserService::UserService()
{
}

bool UserService::findUserByPhone(
    const QString &phone,
    UserInfo &userInfo,
    QString &errorMessage)
{
    QSqlDatabase db = QSqlDatabase::database("ncs_connection");

    if (!db.isValid() || !db.isOpen()) {
        errorMessage = "数据库未打开";
        return false;
    }

    QSqlQuery query(db);

    query.prepare(
        "SELECT id, phone, nickname, avatar_path, "
        "balance, register_time, status "
        "FROM user "
        "WHERE phone = :phone "
        "LIMIT 1"
    );

    query.bindValue(":phone", phone);

    if (!query.exec()) {
        errorMessage =
            "查询用户失败：" + query.lastError().text();
        return false;
    }

    if (!query.next()) {
        return false;
    }

    userInfo.id =
        query.value("id").toInt();

    userInfo.phone =
        query.value("phone").toString();

    userInfo.nickname =
        query.value("nickname").toString();

    userInfo.avatarPath =
        query.value("avatar_path").toString();

    userInfo.balance =
        query.value("balance").toDouble();

    userInfo.registerTime =
        query.value("register_time").toString();

    userInfo.status =
        query.value("status").toInt();

    return true;
}

bool UserService::createUser(
    const QString &phone,
    UserInfo &userInfo,
    QString &errorMessage)
{
    QSqlDatabase db = QSqlDatabase::database("ncs_connection");

    if (!db.isValid() || !db.isOpen()) {
        errorMessage = "数据库未打开";
        return false;
    }

    const QString nickname =
        "用户" + phone.right(4);

    const QString registerTime =
        QDateTime::currentDateTime()
            .toString("yyyy-MM-dd HH:mm:ss");

    if (!db.transaction()) {
        errorMessage =
            "无法开始数据库事务：" +
            db.lastError().text();
        return false;
    }

    QSqlQuery query(db);

    query.prepare(
        "INSERT INTO user "
        "(phone, nickname, avatar_path, balance, "
        "register_time, status) "
        "VALUES "
        "(:phone, :nickname, NULL, 0, "
        ":register_time, 1)"
    );

    query.bindValue(":phone", phone);
    query.bindValue(":nickname", nickname);
    query.bindValue(":register_time", registerTime);

    if (!query.exec()) {
        db.rollback();

        errorMessage =
            "创建用户失败：" +
            query.lastError().text();

        return false;
    }

    if (!db.commit()) {
        db.rollback();

        errorMessage =
            "提交用户创建失败：" +
            db.lastError().text();

        return false;
    }

    userInfo.id =
        query.lastInsertId().toInt();

    userInfo.phone =
        phone;

    userInfo.nickname =
        nickname;

    userInfo.avatarPath =
        QString();

    userInfo.balance =
        0.0;

    userInfo.registerTime =
        registerTime;

    userInfo.status =
        1;

    return true;
}

bool UserService::loginOrRegister(
    const QString &phone,
    const QString &verificationCode,
    const QString &expectedCode,
    UserInfo &userInfo,
    QString &errorMessage)
{
    if (!verificationCode.compare(expectedCode)) {
        // Correct
    } else {
        errorMessage = "验证码错误";
        return false;
    }

    UserInfo existingUser;

    bool querySuccess =
        findUserByPhone(
            phone,
            existingUser,
            errorMessage
        );

    if (!querySuccess) {

        // If there was a real database error,
        // errorMessage will contain text.
        if (!errorMessage.isEmpty()) {
            return false;
        }

        // User doesn't exist.
        return createUser(
            phone,
            userInfo,
            errorMessage
        );
    }

    if (existingUser.status == 0) {
        errorMessage =
            "账号已被冻结，请联系客服";
        return false;
    }

    userInfo = existingUser;

    return true;
}