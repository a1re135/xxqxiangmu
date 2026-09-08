#include "charge_service.h"
#include "util/app_paths.h"

#include <QDateTime>
#include <QDebug>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <cmath>
#include <limits>

namespace {

// 读取充电配置。配置文件不存在时使用默认值。
bool readChargeSettings(
    double &minAmount,
    int &reservationMinutes,
    QString &errorMessage)
{
    minAmount = 5.00;
    reservationMinutes = 15;

    const QString configPath =
        core::resolveDataFile(QStringLiteral("config/app.ini"));

    if (configPath.isEmpty()) {
        return true;
    }

    QSettings settings(configPath, QSettings::IniFormat);

    bool amountOk = false;
    bool minutesOk = false;

    minAmount = settings.value(
        "charge/min_start_amount", 5.00
    ).toDouble(&amountOk);

    reservationMinutes = settings.value(
        "charge/reservation_minutes", 15
    ).toInt(&minutesOk);

    if (settings.status() != QSettings::NoError
        || !amountOk
        || !minutesOk
        || !std::isfinite(minAmount)
        || minAmount < 0
        || reservationMinutes <= 0
        || reservationMinutes
            > std::numeric_limits<int>::max() / 60) {
        errorMessage = QStringLiteral(
            "充电配置无效，请检查最低起充金额和预约分钟数");
        return false;
    }

    return true;
}

bool getChargeDatabase(
    QSqlDatabase &db,
    QString &errorMessage)
{
    db = QSqlDatabase::database("ncs_connection");

    if (!db.isValid() || !db.isOpen()) {
        errorMessage = QStringLiteral("数据库未打开");
        return false;
    }

    return true;
}

bool beginWriteTransaction(
    QSqlDatabase &db,
    QString &errorMessage)
{
    QSqlQuery query(db);

    if (!query.exec("BEGIN IMMEDIATE")) {
        errorMessage = QStringLiteral("无法开始事务：")
                       + query.lastError().text();
        return false;
    }

    return true;
}

bool checkUser(
    QSqlDatabase &db,
    int userId,
    double minAmount,
    QString &errorMessage)
{
    QSqlQuery query(db);

    if (!query.prepare(
            "SELECT status, balance FROM user "
            "WHERE id = :userId")) {
        errorMessage = QStringLiteral("准备用户查询失败：")
                       + query.lastError().text();
        return false;
    }

    query.bindValue(":userId", userId);

    if (!query.exec()) {
        errorMessage = QStringLiteral("查询用户失败：")
                       + query.lastError().text();
        return false;
    }

    if (!query.next()) {
        errorMessage = query.lastError().isValid()
            ? QStringLiteral("读取用户失败：")
              + query.lastError().text()
            : QStringLiteral("用户不存在");
        return false;
    }

    if (query.value("status").toInt() != 1) {
        errorMessage = QStringLiteral(
            "账号已被冻结，请联系客服");
        return false;
    }

    bool balanceOk = false;
    const double balance =
        query.value("balance").toDouble(&balanceOk);

    if (!balanceOk || !std::isfinite(balance) || balance < 0) {
        errorMessage = QStringLiteral("用户余额数据异常");
        return false;
    }

    if (balance < minAmount) {
        errorMessage = QStringLiteral("余额不足，请先充值");
        return false;
    }

    return true;
}

} // namespace

namespace core {

bool ChargeService::reserveCharger(
    int userId,
    int stationId,
    int chargerId,
    int &orderId,
    QString &errorMessage)
{
    orderId = -1;
    errorMessage.clear();

    if (userId <= 0 || stationId <= 0 || chargerId <= 0) {
        errorMessage = QStringLiteral("用户或电桩信息无效");
        return false;
    }

    double minAmount;
    int reservationMinutes;

    if (!readChargeSettings(
            minAmount, reservationMinutes, errorMessage)) {
        return false;
    }

    int timeScale = 60;

    const QString configPath =
        resolveDataFile(QStringLiteral("config/app.ini"));

    if (!configPath.isEmpty()) {
        QSettings settings(configPath, QSettings::IniFormat);

        bool ok = false;
        timeScale = settings.value(
            "charge/time_scale", 60
        ).toInt(&ok);

        if (!ok || timeScale <= 0
            || settings.status() != QSettings::NoError) {
            errorMessage = QStringLiteral("充电时间倍率配置无效");
            return false;
        }
    }

    QSqlDatabase db;
    if (!getChargeDatabase(db, errorMessage)
        || !beginWriteTransaction(db, errorMessage)) {
        return false;
    }

    auto fail = [&](const QString &message) {
        db.rollback();
        errorMessage = message;
        return false;
    };

    QString userError;
    if (!checkUser(db, userId, minAmount, userError)) {
        return fail(userError);
    }

    // 同一事务内检查，防止重复创建未完成订单。
    {
        QSqlQuery existing(db);

        if (!existing.prepare(
                "SELECT id FROM charging_order "
                "WHERE user_id = :userId "
                "AND status IN (0, 1) LIMIT 1")) {
            return fail(
                QStringLiteral("准备订单检查失败：")
                + existing.lastError().text());
        }

        existing.bindValue(":userId", userId);

        if (!existing.exec()) {
            return fail(
                QStringLiteral("检查订单失败：")
                + existing.lastError().text());
        }

        if (existing.next()) {
            return fail(QStringLiteral(
                "您有未完成的充电订单，请先结算"));
        }

        if (existing.lastError().isValid()) {
            return fail(
                QStringLiteral("读取订单失败：")
                + existing.lastError().text());
        }
    }

    // 事务内确认电桩属于该电站、功率与单价有效，
    // 并缓存快照参数供结算使用（UC-U-07、BR-05）。
    QSqlQuery chargerCheck(db);

    if (!chargerCheck.prepare(
            "SELECT c.status, c.power, s.price "
            "FROM charger c "
            "JOIN station s ON s.id = c.station_id "
            "WHERE c.id = :chargerId "
            "AND c.station_id = :stationId")) {
        return fail(
            QStringLiteral("准备查询电桩失败：")
            + chargerCheck.lastError().text());
    }

    chargerCheck.bindValue(":chargerId", chargerId);
    chargerCheck.bindValue(":stationId", stationId);

    if (!chargerCheck.exec()) {
        return fail(
            QStringLiteral("查询电桩失败：")
            + chargerCheck.lastError().text());
    }

    if (!chargerCheck.next()) {
        return fail(QStringLiteral(
            "该电桩不存在或不属于本站，请刷新后重新选择"));
    }

    const int chargerStatus = chargerCheck.value(0).toInt();
    const double power = chargerCheck.value(1).toDouble();
    const double price = chargerCheck.value(2).toDouble();

    if (chargerStatus != 0) {
        return fail(QStringLiteral(
            "该电桩刚被占用、已故障或不可用，请刷新后重新选择"));
    }

    if (!std::isfinite(power) || power <= 0
        || !std::isfinite(price) || price < 0) {
        return fail(QStringLiteral("电桩功率或电站单价异常"));
    }

    chargerCheck.finish();

    // 原子抢占空闲桩：并发场景下只有仍为空闲的电桩
    // 才会被置为使用中（BR-03、UC-U-07 异常流 E2）。
    QSqlQuery occupy(db);

    if (!occupy.prepare(
            "UPDATE charger "
            "SET status = 1 "
            "WHERE id = :chargerId "
            "AND station_id = :stationId "
            "AND status = 0")) {
        return fail(
            QStringLiteral("准备占用电桩失败：")
            + occupy.lastError().text());
    }

    occupy.bindValue(":chargerId", chargerId);
    occupy.bindValue(":stationId", stationId);

    if (!occupy.exec()) {
        return fail(
            QStringLiteral("占用电桩失败：")
            + occupy.lastError().text());
    }

    if (occupy.numRowsAffected() != 1) {
        return fail(QStringLiteral(
            "该电桩刚被占用，请刷新后重新选择"));
    }

    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime expiresAt =
        now.addSecs(reservationMinutes * 60);

    QSqlQuery insertOrder(db);

    if (!insertOrder.prepare(
            "INSERT INTO charging_order "
            "(user_id, charger_id, reserved_at, "
            "reservation_expires_at, start_time, end_time, "
            "energy, amount, status, "
            "unit_price, power_snapshot, time_scale_snapshot) "
            "VALUES (:userId, :chargerId, :reservedAt, "
            ":expiresAt, NULL, NULL, 0, 0, 0, "
            ":unitPrice, :powerSnapshot, :timeScale)")) {
        return fail(
            QStringLiteral("准备创建预约订单失败：")
            + insertOrder.lastError().text());
    }

    insertOrder.bindValue(":userId", userId);
    insertOrder.bindValue(":chargerId", chargerId);
    insertOrder.bindValue(
        ":reservedAt", now.toString("yyyy-MM-dd HH:mm:ss"));
    insertOrder.bindValue(
        ":expiresAt", expiresAt.toString("yyyy-MM-dd HH:mm:ss"));
    insertOrder.bindValue(":unitPrice", price);
    insertOrder.bindValue(":powerSnapshot", power);
    insertOrder.bindValue(":timeScale", timeScale);

    if (!insertOrder.exec()) {
        return fail(
            QStringLiteral("创建预约订单失败：")
            + insertOrder.lastError().text());
    }

    bool idOk = false;
    const int newOrderId =
        insertOrder.lastInsertId().toInt(&idOk);

    if (!idOk || newOrderId <= 0) {
        return fail(QStringLiteral("无法获取预约订单编号"));
    }

    if (!db.commit()) {
        const QString reason = db.lastError().text();
        return fail(QStringLiteral("保存预约失败：") + reason);
    }

    orderId = newOrderId;
    qInfo() << "[ChargeService] Reservation created:"
            << orderId << "charger:" << chargerId;
    return true;
}

bool ChargeService::startReservedCharging(
    int userId,
    int orderId,
    QString &errorMessage)
{
    errorMessage.clear();

    if (userId <= 0 || orderId <= 0) {
        errorMessage = QStringLiteral("用户或订单信息无效");
        return false;
    }

    double minAmount;
    int reservationMinutes;

    if (!readChargeSettings(
            minAmount, reservationMinutes, errorMessage)) {
        return false;
    }

    QSqlDatabase db;
    if (!getChargeDatabase(db, errorMessage)
        || !beginWriteTransaction(db, errorMessage)) {
        return false;
    }

    auto fail = [&](const QString &message) {
        db.rollback();
        errorMessage = message;
        return false;
    };

    QString userError;
    if (!checkUser(db, userId, minAmount, userError)) {
        return fail(userError);
    }

    QSqlQuery query(db);

    if (!query.prepare(
            "SELECT o.status AS order_status, o.start_time, "
            "o.reservation_expires_at, "
            "c.status AS charger_status "
            "FROM charging_order o "
            "JOIN charger c ON c.id = o.charger_id "
            "WHERE o.id = :orderId AND o.user_id = :userId")) {
        return fail(
            QStringLiteral("准备订单查询失败：")
            + query.lastError().text());
    }

    query.bindValue(":orderId", orderId);
    query.bindValue(":userId", userId);

    if (!query.exec()) {
        return fail(
            QStringLiteral("查询订单失败：")
            + query.lastError().text());
    }

    if (!query.next()) {
        return fail(
            query.lastError().isValid()
            ? QStringLiteral("读取订单失败：")
              + query.lastError().text()
            : QStringLiteral("订单不存在或不属于当前用户"));
    }

    if (query.value("order_status").toInt() != 0
        || !query.value("start_time").toString().isEmpty()) {
        return fail(
            QStringLiteral("订单已开始或已结束，请刷新订单"));
    }

    if (query.value("charger_status").toInt() != 1) {
        return fail(
            QStringLiteral("预约电桩状态异常，不能开始充电"));
    }

    const QDateTime expiresAt = QDateTime::fromString(
        query.value("reservation_expires_at").toString(),
        "yyyy-MM-dd HH:mm:ss");

    if (!expiresAt.isValid()) {
        return fail(QStringLiteral(
            "预约到期时间缺失或无效，请取消后重新预约"));
    }

    query.finish();

    const QDateTime now = QDateTime::currentDateTime();

    if (now >= expiresAt) {
        return fail(
            QStringLiteral("预约已超时，请取消后重新预约"));
    }

    QSqlQuery update(db);

    if (!update.prepare(
            "UPDATE charging_order "
            "SET status = 1, start_time = :startTime "
            "WHERE id = :orderId AND user_id = :userId "
            "AND status = 0")) {
        return fail(
            QStringLiteral("准备开始充电失败：")
            + update.lastError().text());
    }

    update.bindValue(
        ":startTime", now.toString("yyyy-MM-dd HH:mm:ss"));
    update.bindValue(":orderId", orderId);
    update.bindValue(":userId", userId);

    if (!update.exec()) {
        return fail(
            QStringLiteral("开始充电失败：")
            + update.lastError().text());
    }

    if (update.numRowsAffected() != 1) {
        return fail(
            QStringLiteral("订单状态已变化，请刷新后重试"));
    }

    if (!db.commit()) {
        const QString reason = db.lastError().text();
        return fail(QStringLiteral("保存充电状态失败：") + reason);
    }

    qInfo() << "[ChargeService] Charging started:" << orderId;
    return true;
}

} // namespace core
