#include "statsservice.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QElapsedTimer>
#include <QMap>
#include <QVariant>

namespace {

QSqlDatabase connection()
{
    return QSqlDatabase::database(QStringLiteral("ncs_connection"));
}

} // namespace

bool StatsService::loadRevenueSummary(
    int days,
    RevenueSummary &outSummary,
    QString &errorMessage) const
{
    errorMessage.clear();
    outSummary = RevenueSummary{};

    const QSqlDatabase db = connection();
    if (!db.isValid() || !db.isOpen()) {
        errorMessage = QStringLiteral("数据库未打开，无法查询营收数据");
        return false;
    }

    // 今日营收 / 订单数
    {
        QSqlQuery query(db);
        query.prepare(
            "SELECT COALESCE(SUM(amount), 0), COUNT(*) "
            "FROM charging_order "
            "WHERE status = 2 AND date(start_time) = date('now', 'localtime')"
        );
        if (!query.exec()) {
            errorMessage = QStringLiteral("查询今日营收失败：") + query.lastError().text();
            return false;
        }
        if (query.next()) {
            outSummary.todayRevenue = query.value(0).toDouble();
            outSummary.todayOrders = query.value(1).toInt();
        }
    }

    // 本月营收 / 订单数
    {
        QSqlQuery query(db);
        query.prepare(
            "SELECT COALESCE(SUM(amount), 0), COUNT(*) "
            "FROM charging_order "
            "WHERE status = 2 "
            "AND strftime('%Y-%m', start_time) = strftime('%Y-%m', 'now', 'localtime')"
        );
        if (!query.exec()) {
            errorMessage = QStringLiteral("查询本月营收失败：") + query.lastError().text();
            return false;
        }
        if (query.next()) {
            outSummary.monthRevenue = query.value(0).toDouble();
            outSummary.monthOrders = query.value(1).toInt();
        }
    }

    // 累计营收 / 订单数
    {
        QSqlQuery query(db);
        query.prepare(
            "SELECT COALESCE(SUM(amount), 0), COUNT(*) "
            "FROM charging_order "
            "WHERE status = 2"
        );
        if (!query.exec()) {
            errorMessage = QStringLiteral("查询累计营收失败：") + query.lastError().text();
            return false;
        }
        if (query.next()) {
            outSummary.totalRevenue = query.value(0).toDouble();
            outSummary.totalOrders = query.value(1).toInt();
        }
    }

    // 近days天（7或30）逐日明细：用于折线图/柱状图。
    // 同时计时这条聚合查询，对应NFR-P-02（30天聚合查询 < 300ms）。
    const int rangeDays = (days == 7) ? 7 : 30;
    const QDate startDate = QDate::currentDate().addDays(-(rangeDays - 1));

    QMap<QString, RevenuePoint> byDate;

    QElapsedTimer timer;
    timer.start();

    {
        QSqlQuery query(db);
        query.prepare(
            "SELECT date(start_time) AS d, SUM(amount), COUNT(*) "
            "FROM charging_order "
            "WHERE status = 2 AND date(start_time) >= :startDate "
            "GROUP BY d "
            "ORDER BY d"
        );
        query.bindValue(":startDate", startDate.toString("yyyy-MM-dd"));

        if (!query.exec()) {
            errorMessage = QStringLiteral("查询营收明细失败：") + query.lastError().text();
            return false;
        }

        while (query.next()) {
            const QString dateStr = query.value(0).toString();
            RevenuePoint point;
            point.date = QDate::fromString(dateStr, "yyyy-MM-dd");
            point.revenue = query.value(1).toDouble();
            point.orderCount = query.value(2).toInt();
            byDate.insert(dateStr, point);
        }
    }

    outSummary.elapsedMs = timer.elapsed();

    // 按连续日期补全，缺失的一天写入revenue=0/orderCount=0，
    // 否则折线图/柱状图的日期轴会出现空洞。
    outSummary.points.reserve(rangeDays);
    for (int i = 0; i < rangeDays; ++i) {
        const QDate day = startDate.addDays(i);
        const QString key = day.toString("yyyy-MM-dd");

        RevenuePoint point;
        point.date = day;

        const auto it = byDate.constFind(key);
        if (it != byDate.constEnd()) {
            point.revenue = it->revenue;
            point.orderCount = it->orderCount;
        }

        outSummary.points.append(point);
    }

    return true;
}

bool StatsService::onlineChargerCount(
    int &outCount,
    QString &errorMessage) const
{
    errorMessage.clear();
    outCount = 0;

    const QSqlDatabase db = connection();
    if (!db.isValid() || !db.isOpen()) {
        errorMessage = QStringLiteral("数据库未打开，无法统计电桩状态");
        return false;
    }

    QSqlQuery query(db);
    query.prepare(
        "SELECT COUNT(*) FROM charger WHERE status != 2"
    );

    if (!query.exec()) {
        errorMessage = QStringLiteral("统计在线电桩失败：") + query.lastError().text();
        return false;
    }

    if (query.next()) {
        outCount = query.value(0).toInt();
    }

    return true;
}


bool StatsService::loadChargerStatusSummary(
    ChargerStatusSummary &outSummary,
    QString &errorMessage) const
{
    errorMessage.clear();
    outSummary = ChargerStatusSummary{};

    const QSqlDatabase db = connection();
    if (!db.isValid() || !db.isOpen()) {
        errorMessage = QStringLiteral("数据库未打开，无法统计电桩状态");
        return false;
    }

    QElapsedTimer timer;
    timer.start();

    // One aggregation query returns the three required charger states.
    QSqlQuery query(db);
    query.prepare(
        "SELECT "
        "SUM(CASE WHEN status = 1 THEN 1 ELSE 0 END), "
        "SUM(CASE WHEN status = 0 THEN 1 ELSE 0 END), "
        "SUM(CASE WHEN status = 2 THEN 1 ELSE 0 END) "
        "FROM charger"
    );

    if (!query.exec()) {
        errorMessage = QStringLiteral("查询电桩状态失败：") + query.lastError().text();
        return false;
    }

    if (query.next()) {
        outSummary.inUse = query.value(0).toInt();
        outSummary.idle = query.value(1).toInt();
        outSummary.fault = query.value(2).toInt();
    }

    outSummary.total = outSummary.inUse + outSummary.idle + outSummary.fault;
    outSummary.healthPercent = outSummary.total > 0
        ? (static_cast<double>(outSummary.inUse + outSummary.idle)
           / static_cast<double>(outSummary.total)) * 100.0
        : 0.0;
    outSummary.elapsedMs = timer.elapsed();

    return true;
}
