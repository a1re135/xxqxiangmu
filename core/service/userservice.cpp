#include "userservice.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QDateTime>

namespace {
bool getOpenDb(QSqlDatabase &db, QString &errorMessage)
{
    db = QSqlDatabase::database("ncs_connection");
    if (!db.isValid() || !db.isOpen()) {
        errorMessage = QStringLiteral("数据库未打开");
        return false;
    }
    return true;
}

void fillUser(QSqlQuery &query, UserInfo &userInfo)
{
    userInfo.id = query.value("id").toInt();
    userInfo.phone = query.value("phone").toString();
    userInfo.nickname = query.value("nickname").toString();
    userInfo.avatarPath = query.value("avatar_path").toString();
    userInfo.balance = query.value("balance").toDouble();
    userInfo.registerTime = query.value("register_time").toString();
    userInfo.status = query.value("status").toInt();
}
}

UserService::UserService() = default;

bool UserService::findUserByPhone(const QString &phone, UserInfo &userInfo, QString &errorMessage)
{
    errorMessage.clear();
    QSqlDatabase db;
    if (!getOpenDb(db, errorMessage)) return false;

    QSqlQuery query(db);
    query.prepare("SELECT id, phone, nickname, avatar_path, balance, register_time, status "
                  "FROM user WHERE phone=:phone LIMIT 1");
    query.bindValue(":phone", phone);

    if (!query.exec()) {
        errorMessage = QStringLiteral("查询用户失败：") + query.lastError().text();
        return false;
    }
    if (!query.next()) return false;

    fillUser(query, userInfo);
    return true;
}

bool UserService::createUser(const QString &phone, UserInfo &userInfo, QString &errorMessage)
{
    errorMessage.clear();
    QSqlDatabase db;
    if (!getOpenDb(db, errorMessage)) return false;

    const QString nickname = QStringLiteral("用户") + phone.right(4);
    const QString registerTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    if (!db.transaction()) {
        errorMessage = QStringLiteral("无法开始数据库事务：") + db.lastError().text();
        return false;
    }

    QSqlQuery query(db);
    query.prepare("INSERT INTO user(phone,nickname,avatar_path,balance,register_time,status) "
                  "VALUES(:phone,:nickname,NULL,0,:register_time,1)");
    query.bindValue(":phone", phone);
    query.bindValue(":nickname", nickname);
    query.bindValue(":register_time", registerTime);

    if (!query.exec()) {
        db.rollback();
        errorMessage = QStringLiteral("创建用户失败：") + query.lastError().text();
        return false;
    }

    const int id = query.lastInsertId().toInt();
    if (!db.commit()) {
        db.rollback();
        errorMessage = QStringLiteral("提交用户创建失败：") + db.lastError().text();
        return false;
    }

    userInfo.id = id;
    userInfo.phone = phone;
    userInfo.nickname = nickname;
    userInfo.avatarPath.clear();
    userInfo.balance = 0.0;
    userInfo.registerTime = registerTime;
    userInfo.status = 1;
    return true;
}

bool UserService::loginOrRegister(const QString &phone,
                                  const QString &verificationCode,
                                  const QString &expectedCode,
                                  UserInfo &userInfo,
                                  QString &errorMessage)
{
    errorMessage.clear();
    if (verificationCode != expectedCode) {
        errorMessage = QStringLiteral("验证码错误");
        return false;
    }

    UserInfo existingUser;
    if (!findUserByPhone(phone, existingUser, errorMessage)) {
        if (!errorMessage.isEmpty()) return false;
        return createUser(phone, userInfo, errorMessage);
    }

    if (existingUser.status == 0) {
        errorMessage = QStringLiteral("账号已被冻结，请联系客服");
        return false;
    }

    userInfo = existingUser;
    return true;
}

bool UserService::getUserById(int id, UserInfo &userInfo, QString &errorMessage)
{
    errorMessage.clear();
    QSqlDatabase db;
    if (!getOpenDb(db, errorMessage)) return false;

    QSqlQuery query(db);
    query.prepare("SELECT id, phone, nickname, avatar_path, balance, register_time, status "
                  "FROM user WHERE id=:id LIMIT 1");
    query.bindValue(":id", id);

    if (!query.exec()) {
        errorMessage = QStringLiteral("查询用户失败：") + query.lastError().text();
        return false;
    }
    if (!query.next()) {
        errorMessage = QStringLiteral("用户不存在");
        return false;
    }

    fillUser(query, userInfo);
    return true;
}

bool UserService::updateAvatar(int id, const QString &avatarPath, QString &errorMessage)
{
    errorMessage.clear();
    QSqlDatabase db;
    if (!getOpenDb(db, errorMessage)) return false;

    QSqlQuery query(db);
    query.prepare("UPDATE user SET avatar_path=:avatar WHERE id=:id");
    query.bindValue(":avatar", avatarPath);
    query.bindValue(":id", id);

    if (!query.exec()) {
        errorMessage = QStringLiteral("修改头像失败：") + query.lastError().text();
        return false;
    }
    if (query.numRowsAffected() != 1) {
        errorMessage = QStringLiteral("修改头像失败：未找到对应用户");
        return false;
    }
    return true;
}

bool UserService::updateNickname(int id, const QString &nickname, QString &errorMessage)
{
    errorMessage.clear();
    const QString cleanNickname = nickname.trimmed();
    if (cleanNickname.isEmpty()) {
        errorMessage = QStringLiteral("昵称不能为空");
        return false;
    }

    QSqlDatabase db;
    if (!getOpenDb(db, errorMessage)) return false;

    // FIX: 昵称必须真正 UPDATE 到数据库；检查受影响行数，避免假成功。
    QSqlQuery query(db);
    query.prepare("UPDATE user SET nickname=:nickname WHERE id=:id");
    query.bindValue(":nickname", cleanNickname);
    query.bindValue(":id", id);

    if (!query.exec()) {
        errorMessage = QStringLiteral("修改昵称失败：") + query.lastError().text();
        return false;
    }
    if (query.numRowsAffected() != 1) {
        errorMessage = QStringLiteral("修改昵称失败：未找到对应用户");
        return false;
    }
    return true;
}

bool UserService::recharge(int id, double amount, double &newBalance, QString &errorMessage)
{
    errorMessage.clear();
    newBalance = 0.0;
    if (amount <= 0.0) {
        errorMessage = QStringLiteral("充值金额必须大于 0");
        return false;
    }

    QSqlDatabase db;
    if (!getOpenDb(db, errorMessage)) return false;

    // FIX: UPDATE + SELECT 放在同一事务中，确保余额更新和页面读到的是同一结果。
    if (!db.transaction()) {
        errorMessage = QStringLiteral("无法开始充值事务：") + db.lastError().text();
        return false;
    }

    QSqlQuery update(db);
    update.prepare("UPDATE user SET balance=ROUND(balance+:money, 2) WHERE id=:id");
    update.bindValue(":money", amount);
    update.bindValue(":id", id);

    if (!update.exec() || update.numRowsAffected() != 1) {
        const QString detail = update.lastError().text();
        db.rollback();
        errorMessage = detail.isEmpty()
            ? QStringLiteral("充值失败：未找到对应用户")
            : QStringLiteral("充值失败：") + detail;
        return false;
    }

    QSqlQuery select(db);
    select.prepare("SELECT balance FROM user WHERE id=:id");
    select.bindValue(":id", id);
    if (!select.exec() || !select.next()) {
        const QString detail = select.lastError().text();
        db.rollback();
        errorMessage = QStringLiteral("读取充值后余额失败：") + detail;
        return false;
    }

    newBalance = select.value(0).toDouble();

    if (!db.commit()) {
        db.rollback();
        errorMessage = QStringLiteral("提交充值失败：") + db.lastError().text();
        return false;
    }
    return true;
}
