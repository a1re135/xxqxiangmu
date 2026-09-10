#include "predictionservice.h"

#include <QDate>
#include <QDateTime>
#include <QMap>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QtGlobal>

#include <cmath>

namespace {

QSqlDatabase connection()
{
    return QSqlDatabase::database(
        QStringLiteral("ncs_connection")
    );
}

QString dateTimeString(const QDateTime &dateTime)
{
    return dateTime.toString(
        QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")
    );
}

} // namespace


bool PredictionService::generatePrediction(
    int stationId,
    int days,
    PredictionSummary &outSummary,
    QString &errorMessage
) const
{
    errorMessage.clear();
    outSummary = PredictionSummary{};

    if (stationId <= 0) {
        errorMessage =
            QStringLiteral("无效的充电站编号。");
        return false;
    }

    const int predictionHours =
        (days == 30) ? 30 : 7;

    QSqlDatabase db = connection();

    if (!db.isValid() || !db.isOpen()) {
        errorMessage =
            QStringLiteral("数据库未打开，无法运行预测。");
        return false;
    }

    // --------------------------------------------------
    // 1. Load station information
    // --------------------------------------------------

    QString stationName;
    int totalChargers = 0;

    {
        QSqlQuery query(db);

        query.prepare(
            "SELECT name "
            "FROM station "
            "WHERE id = :stationId"
        );

        query.bindValue(
            ":stationId",
            stationId
        );

        if (!query.exec()) {
            errorMessage =
                QStringLiteral("查询充电站失败：")
                + query.lastError().text();

            return false;
        }

        if (!query.next()) {
            errorMessage =
                QStringLiteral("找不到指定充电站。");

            return false;
        }

        stationName =
            query.value(0).toString();
    }

    // --------------------------------------------------
    // 2. Count chargers at this station
    // --------------------------------------------------

    {
        QSqlQuery query(db);

        query.prepare(
            "SELECT COUNT(*) "
            "FROM charger "
            "WHERE station_id = :stationId"
        );

        query.bindValue(
            ":stationId",
            stationId
        );

        if (!query.exec()) {
            errorMessage =
                QStringLiteral("统计充电桩失败：")
                + query.lastError().text();

            return false;
        }

        if (query.next()) {
            totalChargers =
                query.value(0).toInt();
        }
    }

    // --------------------------------------------------
    // 3. Read the previous 30 days of completed orders
    // --------------------------------------------------

    const int historyDays = 30;

    const QDate historyStart =
        QDate::currentDate()
            .addDays(-historyDays);

    QMap<QString, double> loadByDate;

    {
        QSqlQuery query(db);

        query.prepare(
            "SELECT "
            "date(o.start_time) AS order_date, "
            "COALESCE(SUM(o.energy), 0) "
            "FROM charging_order o "
            "JOIN charger c "
            "ON o.charger_id = c.id "
            "WHERE o.status = 2 "
            "AND c.station_id = :stationId "
            "AND date(o.start_time) >= :startDate "
            "GROUP BY order_date "
            "ORDER BY order_date"
        );

        query.bindValue(
            ":stationId",
            stationId
        );

        query.bindValue(
            ":startDate",
            historyStart.toString(
                QStringLiteral("yyyy-MM-dd")
            )
        );

        if (!query.exec()) {
            errorMessage =
                QStringLiteral("读取历史充电数据失败：")
                + query.lastError().text();

            return false;
        }

        while (query.next()) {

            const QString date =
                query.value(0).toString();

            const double energy =
                query.value(1).toDouble();

            loadByDate.insert(
                date,
                energy
            );
        }
    }

    // --------------------------------------------------
    // 4. Build a complete 30-day history
    //
    // Missing days are treated as 0 kWh.
    // --------------------------------------------------

    QVector<double> history;

    history.reserve(historyDays);

    for (int i = historyDays; i >= 1; --i) {

        const QDate day =
            QDate::currentDate()
                .addDays(-i);

        const QString key =
            day.toString(
                QStringLiteral("yyyy-MM-dd")
            );

        history.append(
            loadByDate.value(key, 0.0)
        );
    }

    if (history.isEmpty()) {
        errorMessage =
            QStringLiteral("没有足够的历史数据用于预测。");

        return false;
    }

    // --------------------------------------------------
    // 5. Calculate the 30-day average
    // --------------------------------------------------

    double total30 = 0.0;

    for (double value : history) {
        total30 += value;
    }

    const double average30 =
        total30
        / static_cast<double>(history.size());

    // --------------------------------------------------
    // 6. Calculate the most recent 7-day average
    //
    // Recent data receives more influence.
    // --------------------------------------------------

    double total7 = 0.0;

    const int recentCount =
        qMin(7, history.size());

    for (int i =
             history.size() - recentCount;
         i < history.size();
         ++i) {

        total7 += history.at(i);
    }

    const double average7 =
        recentCount > 0
            ? total7
                / static_cast<double>(recentCount)
            : average30;

    // --------------------------------------------------
    // 7. Basic recent trend
    //
    // Compare the latest 7 days with the previous 7 days.
    // --------------------------------------------------

    double previous7Total = 0.0;
    int previous7Count = 0;

    const int previousStart =
        qMax(
            0,
            history.size() - 14
        );

    const int previousEnd =
        qMax(
            0,
            history.size() - 7
        );

    for (int i = previousStart;
         i < previousEnd;
         ++i) {

        previous7Total += history.at(i);
        ++previous7Count;
    }

    const double previous7Average =
        previous7Count > 0
            ? previous7Total
                / static_cast<double>(previous7Count)
            : average7;

    double trendPerDay =
        (average7 - previous7Average) / 7.0;

    // Prevent unrealistic prediction growth.
    const double maxTrend =
        qMax(average30 * 0.08, 1.0);

    trendPerDay =
        qBound(
            -maxTrend,
            trendPerDay,
            maxTrend
        );

    // --------------------------------------------------
    // 8. Weighted starting load
    //
    // 70% recent week
    // 30% full 30-day history
    // --------------------------------------------------

    double baseLoad =
        (average7 * 0.70)
        + (average30 * 0.30);

    if (baseLoad < 0.0) {
        baseLoad = 0.0;
    }

    // --------------------------------------------------
    // 9. Generate future points
    // --------------------------------------------------

    QVector<PredictionPoint> points;

    points.reserve(predictionHours);

    double totalPrediction = 0.0;
    double peakLoad = -1.0;
    int peakIndex = -1;

    for (int i = 0;
         i < predictionHours;
         ++i) {

        const QDate futureDate =
            QDate::currentDate()
                .addDays(i + 1);

        double predicted =
            baseLoad
            + trendPerDay
                * static_cast<double>(i + 1);

        // Give weekends a small demand increase.
        if (futureDate.dayOfWeek() ==
                Qt::Saturday
            || futureDate.dayOfWeek() ==
                Qt::Sunday) {

            predicted *= 1.08;
        }

        predicted =
            qMax(0.0, predicted);

        // Round to 2 decimal places.
        predicted =
            std::round(predicted * 100.0)
            / 100.0;

        PredictionPoint point;

        point.stationId = stationId;
        point.stationName = stationName;

        point.targetTime =
            QDateTime(
                futureDate,
                QTime(12, 0)
            );

        point.predictedLoad =
            predicted;

        /*
         * Rough availability estimate.
         *
         * For this semester simulation we convert
         * expected daily load into a charger usage ratio.
         *
         * It is not claiming to be a real industrial ML
         * occupancy model; it gives the UI a meaningful
         * projected availability value.
         */
        if (totalChargers > 0) {

            const double referenceLoad =
                qMax(baseLoad * 1.6, 1.0);

            const double usageRatio =
                qBound(
                    0.0,
                    predicted / referenceLoad,
                    1.0
                );

            const int estimatedBusy =
                qBound(
                    0,
                    static_cast<int>(
                        std::round(
                            usageRatio
                            * totalChargers
                        )
                    ),
                    totalChargers
                );

            point.predictedFreeChargers =
                totalChargers
                - estimatedBusy;

        } else {

            point.predictedFreeChargers = 0;
        }

        totalPrediction += predicted;

        if (predicted > peakLoad) {
            peakLoad = predicted;
            peakIndex = i;
        }

        points.append(point);
    }

    // --------------------------------------------------
    // 10. Mark peak days
    //
    // Peak = >= 120% of predicted daily average.
    // The absolute maximum is always marked as peak.
    // --------------------------------------------------

    const double predictionAverage =
        predictionHours > 0
            ? totalPrediction
                / static_cast<double>(predictionHours)
            : 0.0;

    for (int i = 0;
         i < points.size();
         ++i) {

        points[i].isPeak =
            points[i].predictedLoad
                >= predictionAverage * 1.20;

        if (i == peakIndex) {
            points[i].isPeak = true;
        }
    }

    // --------------------------------------------------
    // 11. Store this prediction run
    // --------------------------------------------------

    if (!db.transaction()) {

        errorMessage =
            QStringLiteral("无法开始预测数据事务：")
            + db.lastError().text();

        return false;
    }

    const QDateTime generatedTime =
        QDateTime::currentDateTime();

    QSqlQuery insertQuery(db);

    insertQuery.prepare(
        "INSERT INTO load_prediction "
        "("
        "station_id, "
        "generated_time, "
        "target_time, "
        "predicted_load, "
        "predicted_free_chargers, "
        "is_peak"
        ") "
        "VALUES "
        "("
        ":stationId, "
        ":generatedTime, "
        ":targetTime, "
        ":predictedLoad, "
        ":freeChargers, "
        ":isPeak"
        ")"
    );

    for (const PredictionPoint &point : points) {

        insertQuery.bindValue(
            ":stationId",
            stationId
        );

        insertQuery.bindValue(
            ":generatedTime",
            dateTimeString(generatedTime)
        );

        insertQuery.bindValue(
            ":targetTime",
            dateTimeString(point.targetTime)
        );

        insertQuery.bindValue(
            ":predictedLoad",
            point.predictedLoad
        );

        insertQuery.bindValue(
            ":freeChargers",
            point.predictedFreeChargers
        );

        insertQuery.bindValue(
            ":isPeak",
            point.isPeak ? 1 : 0
        );

        if (!insertQuery.exec()) {

            errorMessage =
                QStringLiteral("保存预测结果失败：")
                + insertQuery.lastError().text();

            db.rollback();

            return false;
        }
    }

    if (!db.commit()) {

        errorMessage =
            QStringLiteral("保存预测结果失败：")
            + db.lastError().text();

        db.rollback();

        return false;
    }

    // --------------------------------------------------
    // 12. Build result summary
    // --------------------------------------------------

    outSummary.stationId =
        stationId;

    outSummary.stationName =
        stationName;

    outSummary.predictionHours =
        predictionHours;

    outSummary.totalPredictedLoad =
        totalPrediction;

    outSummary.averageDailyLoad =
        predictionAverage;

    outSummary.peakLoad =
        peakLoad > 0.0
            ? peakLoad
            : 0.0;

    if (peakIndex >= 0
        && peakIndex < points.size()) {

        outSummary.peakTime =
            points.at(peakIndex).targetTime;
    }

    outSummary.points =
        points;

    return true;
}


bool PredictionService::loadLatestPrediction(
    int stationId,
    int hours,
    PredictionSummary &outSummary,
    QString &errorMessage
) const
{
    errorMessage.clear();
    outSummary = PredictionSummary{};

    if (stationId <= 0) {
        errorMessage =
            QStringLiteral("无效的充电站编号。");
        return false;
    }

    const int predictionHours =
        (hours == 1 || hours == 6 || hours == 24)
            ? hours
            : 6;

    QSqlDatabase db = connection();

    if (!db.isValid() || !db.isOpen()) {
        errorMessage =
            QStringLiteral("数据库未打开，无法读取预测结果。");
        return false;
    }

    // --------------------------------------------------
    // Find newest generation timestamp
    // --------------------------------------------------

    QString generatedTime;

    {
        QSqlQuery query(db);

        query.prepare(
            "SELECT generated_time "
            "FROM load_prediction "
            "WHERE station_id = :stationId "
            "GROUP BY generated_time "
            "HAVING COUNT(*) >= :requiredRows "
            "ORDER BY generated_time DESC "
            "LIMIT 1"
        );

        query.bindValue(
            ":stationId",
            stationId
        );

        query.bindValue(
            ":requiredRows",
            predictionHours
        );

        query.bindValue(
            ":stationId",
            stationId
        );

        if (!query.exec()) {

            errorMessage =
                QStringLiteral("读取预测记录失败：")
                + query.lastError().text();

            return false;
        }

        if (query.next()) {
            generatedTime =
                query.value(0).toString();
        }
    }

    if (generatedTime.isEmpty()) {
        errorMessage =
            QStringLiteral("该充电站还没有预测记录。");

        return false;
    }

    // --------------------------------------------------
    // Station name
    // --------------------------------------------------

    QString stationName;

    {
        QSqlQuery query(db);

        query.prepare(
            "SELECT name "
            "FROM station "
            "WHERE id = :stationId"
        );

        query.bindValue(
            ":stationId",
            stationId
        );

        if (!query.exec()
            || !query.next()) {

            errorMessage =
                QStringLiteral("读取充电站信息失败。");

            return false;
        }

        stationName =
            query.value(0).toString();
    }

    // --------------------------------------------------
    // Load prediction points
    // --------------------------------------------------

    QSqlQuery query(db);

    query.prepare(
        "SELECT "
        "target_time, "
        "predicted_load, "
        "predicted_free_chargers, "
        "is_peak "
        "FROM load_prediction "
        "WHERE station_id = :stationId "
        "AND generated_time = :generatedTime "
        "ORDER BY target_time "
        "LIMIT :limit"
    );

    query.bindValue(
        ":stationId",
        stationId
    );

    query.bindValue(
        ":generatedTime",
        generatedTime
    );

    query.bindValue(
        ":limit",
        predictionHours
    );

    if (!query.exec()) {

        errorMessage =
            QStringLiteral("读取预测明细失败：")
            + query.lastError().text();

        return false;
    }

    QVector<PredictionPoint> points;

    double total = 0.0;
    double peakLoad = -1.0;
    QDateTime peakTime;

    while (query.next()) {

        PredictionPoint point;

        point.stationId =
            stationId;

        point.stationName =
            stationName;

        QString text =
            query.value(0).toString();

        point.targetTime =
            QDateTime::fromString(
                text,
                QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")
            );

        if (!point.targetTime.isValid()) {
            point.targetTime =
                QDateTime::fromString(
                    text,
                    QStringLiteral("yyyy-MM-dd HH:mm:ss")
                );
        }

        point.predictedLoad =
            query.value(1).toDouble();

        point.predictedFreeChargers =
            query.value(2).toInt();

        point.isPeak =
            query.value(3).toInt() != 0;

        total += point.predictedLoad;

        if (point.predictedLoad > peakLoad) {

            peakLoad =
                point.predictedLoad;

            peakTime =
                point.targetTime;
        }

        points.append(point);
    }

    if (points.isEmpty()) {

        errorMessage =
            QStringLiteral("预测记录为空。");

        return false;
    }

    outSummary.stationId =
        stationId;

    outSummary.stationName =
        stationName;

    outSummary.predictionHours =
        points.size();

    outSummary.totalPredictedLoad =
        total;

    outSummary.averageDailyLoad =
        total
        / static_cast<double>(
            points.size()
        );

    outSummary.peakLoad =
        qMax(0.0, peakLoad);

    outSummary.peakTime =
        peakTime;

    outSummary.points =
        points;

    return true;
}
