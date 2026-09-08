#pragma once

#include <QString>
#include <QVector>

namespace core {

struct ChargeOrderInfo
{
    int id = -1;
    int status = -1;

    QString stationName;
    QString chargerNo;
    QString startTime;
    QString reservationExpiresAt;

    double power = 0;
    double price = 0;
    double energy = 0;
    double amount = 0;

    qint64 simulatedSeconds = 0;
};

// 结算结果（UC-U-09），供结算小票展示使用。
struct SettlementResult
{
    int orderId = -1;
    qint64 simulatedSeconds = 0;

    double energy = 0;
    double amount = 0;
    double paidAmount = 0;
    double debtAmount = 0;
    double balanceAfter = 0;

    QString endTime;
};

// 我的订单列表条目（UC-U-10），倒序返回。
struct OrderHistoryItem
{
    int id = -1;
    int status = -1;

    QString stationName;
    QString chargerNo;
    QString startTime;
    QString endTime;

    double energy = 0;
    double amount = 0;
    double unitPrice = 0;

    qint64 simulatedSeconds = 0;
};

// 订单小票详情（UC-U-09/10）。
// 充电中的订单 energy/amount 为当前实时值，
// paidAmount/debtAmount/balanceAfter 仅在已结算订单上有效。
struct OrderReceipt
{
    int orderId = -1;
    int status = -1;

    QString stationName;
    QString chargerNo;
    QString startTime;
    QString endTime;

    qint64 simulatedSeconds = 0;

    double energy = 0;
    double amount = 0;
    double unitPrice = 0;

    double paidAmount = -1;
    double debtAmount = -1;
    double balanceAfter = -1;
};

class ChargeService
{
public:

    bool ensureChargingSnapshot(
        int userId,
        int orderId,
        QString &errorMessage
    );

    bool getOrderInfo(
        int userId,
        int orderId,
        ChargeOrderInfo &info,
        QString &errorMessage
    );

    bool finishCharging(
        int userId,
        int orderId,
        SettlementResult &result,
        QString &errorMessage
    );

    // 预约空闲电桩，成功后返回预约订单编号。
    bool reserveCharger(
        int userId,
        int stationId,
        int chargerId,
        int &orderId,
        QString &errorMessage
    );

    // 将已有预约订单转换为充电中订单。
    bool startReservedCharging(
        int userId,
        int orderId,
        QString &errorMessage
    );

    // 当前用户的历史订单列表，按订单编号倒序（UC-U-10）。
    bool listOrders(
        int userId,
        QVector<OrderHistoryItem> &items,
        QString &errorMessage
    );

    // 订单小票详情（UC-U-09/10）。
    bool getReceipt(
        int userId,
        int orderId,
        OrderReceipt &receipt,
        QString &errorMessage
    );
};

} // namespace core
