#ifndef STATSSERVICE_H
#define STATSSERVICE_H

#include <QString>
#include <QVector>
#include <QDate>

// Statistics/query service used by the admin dashboard.
class StatsService
{
public:
    struct RevenuePoint
    {
        QDate date;
        double revenue = 0.0;
        int orderCount = 0;
    };

    struct RevenueSummary
    {
        double todayRevenue = 0.0;
        double monthRevenue = 0.0;
        double totalRevenue = 0.0;

        int todayOrders = 0;
        int monthOrders = 0;
        int totalOrders = 0;

        qint64 elapsedMs = 0;
        QVector<RevenuePoint> points;
    };

    struct ChargerStatusSummary
    {
        int inUse = 0;
        int idle = 0;
        int fault = 0;
        int total = 0;
        double healthPercent = 0.0;

        qint64 elapsedMs = 0;
    };

    bool loadRevenueSummary(
        int days,
        RevenueSummary &outSummary,
        QString &errorMessage
    ) const;

    bool onlineChargerCount(
        int &outCount,
        QString &errorMessage
    ) const;

    bool loadChargerStatusSummary(
        ChargerStatusSummary &outSummary,
        QString &errorMessage
    ) const;
};

#endif // STATSSERVICE_H
