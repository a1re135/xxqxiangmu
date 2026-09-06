#ifndef STATIONSERVICE_H
#define STATIONSERVICE_H

#include <QList>
#include <QString>

class StationService
{
public:
    struct StationRecord
    {
        int id = 0;
        QString name;
        QString address;
        double longitude = 0.0;
        double latitude = 0.0;
        double price = 0.0;
        int totalChargers = 0;
        int nonFaultChargers = 0;
        double onlineRate = 0.0;
    };

    struct ChargerDetail
    {
        int id = 0;
        QString chargerNo;
        int type = 0;
        double power = 0.0;
        int status = 0;
        int totalCount = 0;
        int totalMinutes = 0;
    };

    bool loadStations(QList<StationRecord> &out, QString &errorMessage) const;
    bool loadChargersForStation(int stationId,
                                QList<ChargerDetail> &out,
                                QString &errorMessage) const;

    bool addStation(const QString &name,
                    const QString &address,
                    double longitude,
                    double latitude,
                    double price,
                    int initialChargerCount,
                    double defaultPower,
                    QString &errorMessage) const;

    bool updateStation(int stationId,
                       const QString &name,
                       const QString &address,
                       double longitude,
                       double latitude,
                       double price,
                       QString &errorMessage) const;

    bool deleteStation(int stationId, QString &errorMessage) const;
};

#endif // STATIONSERVICE_H
