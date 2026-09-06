#ifndef CHARGERSERVICE_H
#define CHARGERSERVICE_H

#include <QList>
#include <QString>

class ChargerService
{
public:
    struct ChargerRecord
    {
        int id = 0;
        int stationId = 0;
        QString chargerNo;
        QString stationName;
        int type = 0;          // 0 slow, 1 fast
        double power = 0.0;
        int status = 0;        // 0 idle, 1 in use, 2 fault
        int totalCount = 0;
        int totalMinutes = 0;
    };

    struct StationOption
    {
        int id = 0;
        QString name;
    };

    bool loadChargers(QList<ChargerRecord> &out, QString &errorMessage) const;
    bool loadStations(QList<StationOption> &out, QString &errorMessage) const;

    bool setFault(int chargerId, QString &errorMessage) const;
    bool recover(int chargerId, QString &errorMessage) const;

    bool restartCharger(int chargerId, QString &errorMessage) const;

    bool addCharger(int stationId,
                    const QString &chargerNo,
                    int type,
                    double power,
                    QString &errorMessage) const;

    bool deleteCharger(int chargerId, QString &errorMessage) const;
};

#endif // CHARGERSERVICE_H
