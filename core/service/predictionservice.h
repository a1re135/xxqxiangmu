#ifndef PREDICTIONSERVICE_H
#define PREDICTIONSERVICE_H

#include <QDateTime>
#include <QString>
#include <QVector>

class PredictionService
{
public:
    struct PredictionPoint
    {
        int stationId = 0;
        QString stationName;

        QDateTime targetTime;

        double predictedLoad = 0.0;
        int predictedFreeChargers = 0;

        bool isPeak = false;
    };

    struct PredictionSummary
    {
        int stationId = 0;
        QString stationName;

        int predictionDays = 7;

        double totalPredictedLoad = 0.0;
        double averageDailyLoad = 0.0;

        double peakLoad = 0.0;
        QDateTime peakTime;

        QVector<PredictionPoint> points;
    };

    bool generatePrediction(
        int stationId,
        int days,
        PredictionSummary &outSummary,
        QString &errorMessage
    ) const;

    bool loadLatestPrediction(
        int stationId,
        int days,
        PredictionSummary &outSummary,
        QString &errorMessage
    ) const;
};

#endif // PREDICTIONSERVICE_H
