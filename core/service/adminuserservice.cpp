#include "adminuserservice.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace {
QSqlDatabase database()
{
    return QSqlDatabase::database("ncs_connection");
}

bool ensureDatabaseOpen(QString &errorMessage)
{
    const QSqlDatabase db = database();
    if (!db.isValid() || !db.isOpen()) {
        errorMessage = QStringLiteral("数据库未打开");
        return false;
    }
    return true;
}
}

bool AdminUserService::loadUsers(const QString &phoneKeyword,
                                 QList<UserRecord> &users,
                                 QString &errorMessage) const
{
    users.clear();
    if (!ensureDatabaseOpen(errorMessage)) {
        return false;
    }

    const QSqlDatabase db = database();
    QSqlQuery query(db);
    query.prepare(
        "SELECT id, phone, nickname, balance, register_time, status "
        "FROM user "
        "WHERE phone LIKE :phone "
        "ORDER BY id ASC"
    );
    query.bindValue(":phone", "%" + phoneKeyword.trimmed() + "%");

    if (!query.exec()) {
        errorMessage = QStringLiteral("查询用户失败：") + query.lastError().text();
        return false;
    }

    while (query.next()) {
        UserRecord u;
        u.id = query.value(0).toInt();
        u.phone = query.value(1).toString();
        u.nickname = query.value(2).toString();
        u.balance = query.value(3).toDouble();
        u.registerTime = query.value(4).toString();
        u.status = query.value(5).toInt();
        users.append(u);
    }
    return true;
}

bool AdminUserService::hasActiveChargingOrder(int userId,
                                               bool &hasActive,
                                               QString &errorMessage) const
{
    hasActive = false;
    if (!ensureDatabaseOpen(errorMessage)) {
        return false;
    }

    const QSqlDatabase db = database();
    QSqlQuery query(db);
    query.prepare(
        "SELECT COUNT(*) "
        "FROM charging_order "
        "WHERE user_id = :user_id AND status = 0"
    );
    query.bindValue(":user_id", userId);

    if (!query.exec()) {
        errorMessage = QStringLiteral("检查充电中订单失败：") + query.lastError().text();
        return false;
    }

    if (query.next()) {
        hasActive = query.value(0).toInt() > 0;
    }
    return true;
}

bool AdminUserService::setUserStatus(int userId,
                                     int status,
                                     QString &errorMessage) const
{
    if (!ensureDatabaseOpen(errorMessage)) {
        return false;
    }

    const QSqlDatabase db = database();
    QSqlQuery query(db);
    query.prepare("UPDATE user SET status = :status WHERE id = :id");
    query.bindValue(":status", status);
    query.bindValue(":id", userId);

    if (!query.exec()) {
        errorMessage = QStringLiteral("更新用户状态失败：") + query.lastError().text();
        return false;
    }

    if (query.numRowsAffected() == 0) {
        errorMessage = QStringLiteral("未找到要更新的用户。");
        return false;
    }
    return true;
}

bool AdminUserService::loadUserOrderHistory(int userId,
                                            QList<OrderRecord> &orders,
                                            QString &errorMessage) const
{
    orders.clear();
    if (!ensureDatabaseOpen(errorMessage)) {
        return false;
    }

    const QSqlDatabase db = database();
    QSqlQuery query(db);
    query.prepare(
        "SELECT o.id, c.charger_no, s.name, o.start_time, o.end_time, "
        "o.energy, o.amount, o.status "
        "FROM charging_order o "
        "LEFT JOIN charger c ON c.id = o.charger_id "
        "LEFT JOIN station s ON s.id = c.station_id "
        "WHERE o.user_id = :user_id "
        "ORDER BY o.id DESC"
    );
    query.bindValue(":user_id", userId);

    if (!query.exec()) {
        errorMessage = QStringLiteral("查询订单历史失败：") + query.lastError().text();
        return false;
    }

    while (query.next()) {
        OrderRecord o;
        o.id = query.value(0).toInt();
        o.chargerNo = query.value(1).toString();
        o.stationName = query.value(2).toString();
        o.startTime = query.value(3).toString();
        o.endTime = query.value(4).toString();
        o.energy = query.value(5).toDouble();
        o.amount = query.value(6).toDouble();
        o.status = query.value(7).toInt();
        orders.append(o);
    }

    return true;
}
