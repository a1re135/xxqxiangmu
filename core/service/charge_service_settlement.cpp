#include "charge_service.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <cmath>
#include <limits>

namespace core {

bool ChargeService::finishCharging(
    int userId,
    int orderId,
    SettlementResult &result,
    QString &errorMessage)
{
    result = SettlementResult{};
    errorMessage.clear();

    if (userId <= 0 || orderId <= 0) {
        errorMessage = QStringLiteral("用户或订单信息无效");
        return false;
    }

    // 兼容旧订单：缺少计费参数时先补齐。
    // 此处不扣款、不结束订单。
    ChargeOrderInfo preview;
    if (!getOrderInfo(userId, orderId, preview, errorMessage)) {
        return false;
    }

    if (preview.status != 1) {
        errorMessage = QStringLiteral(
            "订单已结束或尚未开始充电，请刷新");
        return false;
    }

    QSqlDatabase db =
        QSqlDatabase::database("ncs_connection");

    if (!db.isValid() || !db.isOpen()) {
        errorMessage = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery begin(db);

    if (!begin.exec("BEGIN IMMEDIATE")) {
        errorMessage = QStringLiteral("结算失败，请重试：")
                       + begin.lastError().text();
        return false;
    }

    auto fail = [&](const QString &reason) {
        db.rollback();
        errorMessage =
            QStringLiteral("结算失败，请重试：") + reason;
        return false;
    };

    // 在写锁保护下重新读取，避免重复结算和余额竞争。
    QSqlQuery query(db);

    if (!query.prepare(
            "SELECT o.status, o.charger_id, o.start_time, "
            "o.unit_price, o.power_snapshot, "
            "o.time_scale_snapshot, u.balance "
            "FROM charging_order o "
            "JOIN user u ON u.id = o.user_id "
            "JOIN charger c ON c.id = o.charger_id "
            "WHERE o.id = :orderId AND o.user_id = :userId")) {
        return fail(query.lastError().text());
    }

    query.bindValue(":orderId", orderId);
    query.bindValue(":userId", userId);

    if (!query.exec()) {
        return fail(query.lastError().text());
    }

    if (!query.next()) {
        return fail(
            query.lastError().isValid()
            ? query.lastError().text()
            : QStringLiteral("订单不存在或关联数据缺失"));
    }

    if (query.value("status").toInt() != 1) {
        return fail(
            QStringLiteral("订单已被处理，不能重复结算"));
    }

    const int chargerId = query.value("charger_id").toInt();

    const QDateTime start = QDateTime::fromString(
        query.value("start_time").toString(),
        "yyyy-MM-dd HH:mm:ss"
    );

    bool priceOk = false;
    bool powerOk = false;
    bool scaleOk = false;
    bool balanceOk = false;

    const double price =
        query.value("unit_price").toDouble(&priceOk);
    const double power =
        query.value("power_snapshot").toDouble(&powerOk);
    const int scale =
        query.value("time_scale_snapshot").toInt(&scaleOk);
    const double balance =
        query.value("balance").toDouble(&balanceOk);

    query.finish();

    if (!start.isValid()
        || !priceOk || !powerOk || !scaleOk || !balanceOk
        || !std::isfinite(price)
        || !std::isfinite(power)
        || !std::isfinite(balance)
        || price < 0 || power <= 0
        || scale <= 0 || balance < 0) {
        return fail(QStringLiteral("订单计费参数或余额异常"));
    }

    // 最终费用和结束时间使用同一个时间点。
    const QDateTime end = QDateTime::currentDateTime();
    const qint64 realSeconds = start.secsTo(end);

    if (realSeconds < 0
        || realSeconds
            > std::numeric_limits<qint64>::max() / scale) {
        return fail(QStringLiteral("充电时长异常"));
    }

    SettlementResult settled;
    settled.orderId = orderId;
    settled.endTime = end.toString("yyyy-MM-dd HH:mm:ss");
    settled.simulatedSeconds = realSeconds * scale;
    settled.energy =
        power * (settled.simulatedSeconds / 3600.0);

    const double rawAmount = settled.energy * price;

    // 限制到可可靠转换为整数分的范围。
    constexpr double maxMoney = 9000000000000.0;

    if (!std::isfinite(settled.energy)
        || !std::isfinite(rawAmount)
        || rawAmount < 0
        || rawAmount > maxMoney
        || balance > maxMoney) {
        return fail(QStringLiteral("金额或电量超出有效范围"));
    }

    // 先转成整数“分”，再进行扣款和欠费计算。
    const qint64 amountCents =
        static_cast<qint64>(std::llround(rawAmount * 100.0));
    const qint64 balanceCents =
        static_cast<qint64>(std::llround(balance * 100.0));
    const qint64 paidCents =
        amountCents < balanceCents ? amountCents : balanceCents;

    settled.amount = amountCents / 100.0;
    settled.paidAmount = paidCents / 100.0;
    settled.debtAmount = (amountCents - paidCents) / 100.0;
    settled.balanceAfter = (balanceCents - paidCents) / 100.0;

    // 1. 保存订单最终结果。
    QSqlQuery orderUpdate(db);

    if (!orderUpdate.prepare(
            "UPDATE charging_order SET "
            "status = 2, end_time = :endTime, "
            "energy = :energy, amount = :amount, "
            "paid_amount = :paid, debt_amount = :debt, "
            "balance_after = :balanceAfter, "
            "simulated_seconds = :seconds "
            "WHERE id = :orderId AND user_id = :userId "
            "AND status = 1")) {
        return fail(orderUpdate.lastError().text());
    }

    orderUpdate.bindValue(":endTime", settled.endTime);
    orderUpdate.bindValue(":energy", settled.energy);
    orderUpdate.bindValue(":amount", settled.amount);
    orderUpdate.bindValue(":paid", settled.paidAmount);
    orderUpdate.bindValue(":debt", settled.debtAmount);
    orderUpdate.bindValue(":balanceAfter", settled.balanceAfter);
    orderUpdate.bindValue(":seconds", settled.simulatedSeconds);
    orderUpdate.bindValue(":orderId", orderId);
    orderUpdate.bindValue(":userId", userId);

    if (!orderUpdate.exec()) {
        return fail(orderUpdate.lastError().text());
    }

    if (orderUpdate.numRowsAffected() != 1) {
        return fail(QStringLiteral("订单状态已变化"));
    }

    // 2. 扣减用户余额，余额不足时扣至0。
    // 被冻结的用户仍允许结算已有订单。
    QSqlQuery userUpdate(db);

    if (!userUpdate.prepare(
            "UPDATE user SET balance = :balance "
            "WHERE id = :userId")) {
        return fail(userUpdate.lastError().text());
    }

    userUpdate.bindValue(":balance", settled.balanceAfter);
    userUpdate.bindValue(":userId", userId);

    if (!userUpdate.exec()) {
        return fail(userUpdate.lastError().text());
    }

    if (userUpdate.numRowsAffected() != 1) {
        return fail(QStringLiteral("更新用户余额失败"));
    }

    // 3. 释放电桩并累计统计。
    // 保留管理端设置的故障状态，也不释放其他有效订单的桩。
    QSqlQuery chargerUpdate(db);

    if (!chargerUpdate.prepare(
            "UPDATE charger SET "
            "status = CASE "
            " WHEN status = 1 AND NOT EXISTS ("
            "  SELECT 1 FROM charging_order o "
            "  WHERE o.charger_id = charger.id "
            "  AND o.status IN (0, 1)"
            " ) THEN 0 ELSE status END, "
            "total_count = COALESCE(total_count, 0) + 1, "
            "total_minutes = COALESCE(total_minutes, 0) "
            "                + :minutes "
            "WHERE id = :chargerId")) {
        return fail(chargerUpdate.lastError().text());
    }

    // 本项目累计的是模拟充电分钟数，不足一分钟舍去。
    chargerUpdate.bindValue(
        ":minutes", settled.simulatedSeconds / 60);
    chargerUpdate.bindValue(":chargerId", chargerId);

    if (!chargerUpdate.exec()) {
        return fail(chargerUpdate.lastError().text());
    }

    if (chargerUpdate.numRowsAffected() != 1) {
        return fail(QStringLiteral("更新电桩统计失败"));
    }

    if (!db.commit()) {
        const QString reason = db.lastError().text();
        return fail(reason);
    }

    // 只有整个事务提交成功，才返回结算结果。
    result = settled;

    qInfo() << "[ChargeService] Order settled:"
            << orderId
            << "amount:" << result.amount
            << "paid:" << result.paidAmount
            << "debt:" << result.debtAmount;

    return true;
}

} // namespace core
