#include "charge_service.h"
#include "util/app_paths.h"

#include <QDateTime>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <cmath>
#include <limits>

namespace core {

bool ChargeService::getOrderInfo(
    int userId,
    int orderId,
    ChargeOrderInfo &info,
    QString &errorMessage)
{
    info = ChargeOrderInfo{};
    errorMessage.clear();

    if (userId <= 0 || orderId <= 0) {
        errorMessage = QStringLiteral("用户或订单信息无效");
        return false;
    }

    QSqlDatabase db =
        QSqlDatabase::database("ncs_connection");

    if (!db.isValid() || !db.isOpen()) {
        errorMessage = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery query(db);

    if (!query.prepare(
            "SELECT o.id, o.status, o.start_time, "
            "o.reservation_expires_at, "
            "o.energy, o.amount, "
            "o.unit_price, o.power_snapshot, o.time_scale_snapshot, "
            "s.name AS station_name, "
            "c.charger_no "
            "FROM charging_order o "
            "LEFT JOIN charger c ON c.id = o.charger_id "
            "LEFT JOIN station s ON s.id = c.station_id "
            "WHERE o.id = :orderId "
            "AND o.user_id = :userId")) {
        errorMessage = QStringLiteral("准备订单查询失败：")
                       + query.lastError().text();
        return false;
    }

    query.bindValue(":orderId", orderId);
    query.bindValue(":userId", userId);

    if (!query.exec()) {
        errorMessage = QStringLiteral("查询订单失败：")
                       + query.lastError().text();
        return false;
    }

    if (!query.next()) {
        errorMessage = query.lastError().isValid()
            ? QStringLiteral("读取订单失败：")
              + query.lastError().text()
            : QStringLiteral("订单不存在或不属于当前用户");
        return false;
    }

    info.id = query.value("id").toInt();
    info.status = query.value("status").toInt();
    info.stationName = query.value("station_name").toString();
    info.chargerNo = query.value("charger_no").toString();
    info.startTime = query.value("start_time").toString();
    info.reservationExpiresAt =
        query.value("reservation_expires_at").toString();
    info.energy = query.value("energy").toDouble();
    info.amount = query.value("amount").toDouble();

    // 历史订单的最终电量和金额直接使用数据库值。
    if (info.status != 1) {
        info.price = query.value("unit_price").toDouble();
        info.power = query.value("power_snapshot").toDouble();
        return true;
    }

    // 兼容升级前已经开始的订单，只补齐一次。
    if (query.value("unit_price").isNull()
        || query.value("power_snapshot").isNull()
        || query.value("time_scale_snapshot").isNull()) {
        query.finish();

        if (!ensureChargingSnapshot(
                userId, orderId, errorMessage)) {
            return false;
        }

        // 保存后重新读取固定参数。
        return getOrderInfo(
            userId, orderId, info, errorMessage);
    }

    bool powerOk = false;
    bool priceOk = false;
    bool scaleOk = false;

    info.power =
        query.value("power_snapshot").toDouble(&powerOk);
    info.price =
        query.value("unit_price").toDouble(&priceOk);

    const int timeScale =
        query.value("time_scale_snapshot").toInt(&scaleOk);

    if (!scaleOk || timeScale <= 0) {
        errorMessage = QStringLiteral("订单时间倍率异常");
        return false;
    }

    if (!powerOk || !priceOk
        || !std::isfinite(info.power)
        || !std::isfinite(info.price)
        || info.power <= 0
        || info.price < 0) {
        errorMessage = QStringLiteral("电桩功率或电站单价异常");
        return false;
    }

    const QDateTime start = QDateTime::fromString(
        info.startTime,
        "yyyy-MM-dd HH:mm:ss"
    );

    if (!start.isValid()) {
        errorMessage = QStringLiteral("订单开始时间无效");
        return false;
    }




    const qint64 realSeconds =
        start.secsTo(QDateTime::currentDateTime());

    if (realSeconds < 0) {
        errorMessage = QStringLiteral(
            "系统时间早于订单开始时间，请检查系统时钟");
        return false;
    }

    if (realSeconds
        > std::numeric_limits<qint64>::max() / timeScale) {
        errorMessage = QStringLiteral("充电时长超出有效范围");
        return false;
    }

    info.simulatedSeconds = realSeconds * timeScale;

    // 功率(kW) × 时间(h) = 电量(kWh)。
    info.energy =
        info.power * (info.simulatedSeconds / 3600.0);

    const double rawAmount = info.energy * info.price;

    if (!std::isfinite(info.energy)
        || !std::isfinite(rawAmount)
        || !std::isfinite(rawAmount * 100.0)) {
        errorMessage = QStringLiteral("计费结果超出有效范围");
        return false;
    }

    // 费用保留两位小数。
    info.amount = std::round(rawAmount * 100.0) / 100.0;

    return true;
}

bool ChargeService::listOrders(
    int userId,
    QVector<OrderHistoryItem> &items,
    QString &errorMessage)
{
    items.clear();
    errorMessage.clear();

    if (userId <= 0) {
        errorMessage = QStringLiteral("用户未登录");
        return false;
    }

    QSqlDatabase db =
        QSqlDatabase::database("ncs_connection");

    if (!db.isValid() || !db.isOpen()) {
        errorMessage = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery query(db);

    // 电站名与电桩编号通过 JOIN 关联获得；
    // 按订单编号倒序展示（UC-U-10）。
    if (!query.prepare(
            "SELECT o.id, o.status, o.start_time, o.end_time, "
            "o.energy, o.amount, o.unit_price, "
            "o.simulated_seconds, "
            "s.name AS station_name, "
            "c.charger_no "
            "FROM charging_order o "
            "LEFT JOIN charger c ON c.id = o.charger_id "
            "LEFT JOIN station s ON s.id = c.station_id "
            "WHERE o.user_id = :userId "
            "ORDER BY o.id DESC")) {
        errorMessage = QStringLiteral("准备订单列表查询失败：")
                       + query.lastError().text();
        return false;
    }

    query.bindValue(":userId", userId);

    if (!query.exec()) {
        errorMessage = QStringLiteral("查询订单列表失败：")
                       + query.lastError().text();
        return false;
    }

    while (query.next()) {
        OrderHistoryItem item;
        item.id = query.value("id").toInt();
        item.status = query.value("status").toInt();
        item.stationName = query.value("station_name").toString();
        item.chargerNo = query.value("charger_no").toString();
        item.startTime = query.value("start_time").toString();
        item.endTime = query.value("end_time").toString();
        item.energy = query.value("energy").toDouble();
        item.amount = query.value("amount").toDouble();
        item.unitPrice = query.value("unit_price").toDouble();
        item.simulatedSeconds =
            query.value("simulated_seconds").toLongLong();
        items.append(item);
    }

    if (query.lastError().isValid()) {
        errorMessage = QStringLiteral("读取订单列表失败：")
                       + query.lastError().text();
        return false;
    }

    return true;
}

bool ChargeService::getReceipt(
    int userId,
    int orderId,
    OrderReceipt &receipt,
    QString &errorMessage)
{
    receipt = OrderReceipt{};
    errorMessage.clear();

    if (userId <= 0 || orderId <= 0) {
        errorMessage = QStringLiteral("用户或订单信息无效");
        return false;
    }

    QSqlDatabase db =
        QSqlDatabase::database("ncs_connection");

    if (!db.isValid() || !db.isOpen()) {
        errorMessage = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery query(db);

    if (!query.prepare(
            "SELECT o.id, o.status, o.start_time, o.end_time, "
            "o.energy, o.amount, o.unit_price, o.power_snapshot, "
            "o.time_scale_snapshot, o.simulated_seconds, "
            "o.paid_amount, o.debt_amount, o.balance_after, "
            "s.name AS station_name, "
            "c.charger_no "
            "FROM charging_order o "
            "LEFT JOIN charger c ON c.id = o.charger_id "
            "LEFT JOIN station s ON s.id = c.station_id "
            "WHERE o.id = :orderId "
            "AND o.user_id = :userId")) {
        errorMessage = QStringLiteral("准备小票查询失败：")
                       + query.lastError().text();
        return false;
    }

    query.bindValue(":orderId", orderId);
    query.bindValue(":userId", userId);

    if (!query.exec()) {
        errorMessage = QStringLiteral("查询小票失败：")
                       + query.lastError().text();
        return false;
    }

    if (!query.next()) {
        errorMessage = query.lastError().isValid()
            ? QStringLiteral("读取小票失败：")
              + query.lastError().text()
            : QStringLiteral("订单不存在或不属于当前用户");
        return false;
    }

    receipt.orderId = query.value("id").toInt();
    receipt.status = query.value("status").toInt();
    receipt.stationName = query.value("station_name").toString();
    receipt.chargerNo = query.value("charger_no").toString();
    receipt.startTime = query.value("start_time").toString();
    receipt.endTime = query.value("end_time").toString();
    receipt.energy = query.value("energy").toDouble();
    receipt.amount = query.value("amount").toDouble();
    receipt.unitPrice = query.value("unit_price").toDouble();
    receipt.simulatedSeconds =
        query.value("simulated_seconds").toLongLong();

    if (receipt.status == 1) {
        // We are done reading this query before performing
        // another query through getOrderInfo().
        query.finish();

        ChargeOrderInfo info;

        if (!getOrderInfo(
                userId,
                orderId,
                info,
                errorMessage)) {
            return false;
        }

        receipt.energy = info.energy;
        receipt.amount = info.amount;
        receipt.unitPrice = info.price;
        receipt.simulatedSeconds = info.simulatedSeconds;

        receipt.paidAmount = -1;
        receipt.debtAmount = -1;
        receipt.balanceAfter = -1;

        return true;
    }

    // Read settlement fields BEFORE query.finish().
    receipt.paidAmount =
        query.value("paid_amount").toDouble();

    receipt.debtAmount =
        query.value("debt_amount").toDouble();

    receipt.balanceAfter =
        query.value("balance_after").toDouble();

    // Now we are finished with the result.
    query.finish();

    // 历史种子订单没有 simulated_seconds，
    // 用开始/结束时间差补算充电时长。
    if (receipt.simulatedSeconds <= 0
        && !receipt.startTime.isEmpty()
        && !receipt.endTime.isEmpty()) {
        const QDateTime start = QDateTime::fromString(
            receipt.startTime, "yyyy-MM-dd HH:mm:ss");
        const QDateTime end = QDateTime::fromString(
            receipt.endTime, "yyyy-MM-dd HH:mm:ss");

        if (start.isValid() && end.isValid()) {
            receipt.simulatedSeconds = start.secsTo(end);
        }
    }

    return true;
}

} // namespace core
