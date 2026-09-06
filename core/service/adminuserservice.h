#ifndef ADMINUSERSERVICE_H
#define ADMINUSERSERVICE_H

#include <QString>
#include <QList>

class AdminUserService
{
public:
    struct UserRecord {
        int id = 0;
        QString phone;
        QString nickname;
        double balance = 0.0;
        QString registerTime;
        int status = 1;
    };

    struct OrderRecord {
        int id = 0;
        QString chargerNo;
        QString stationName;
        QString startTime;
        QString endTime;
        double energy = 0.0;
        double amount = 0.0;
        int status = 0;
    };

    bool loadUsers(const QString &phoneKeyword,
                   QList<UserRecord> &users,
                   QString &errorMessage) const;

    bool hasActiveChargingOrder(int userId,
                                bool &hasActive,
                                QString &errorMessage) const;

    bool setUserStatus(int userId,
                       int status,
                       QString &errorMessage) const;

    bool loadUserOrderHistory(int userId,
                              QList<OrderRecord> &orders,
                              QString &errorMessage) const;
};

#endif // ADMINUSERSERVICE_H
