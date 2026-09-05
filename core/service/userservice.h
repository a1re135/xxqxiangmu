#ifndef USERSERVICE_H
#define USERSERVICE_H

#include <QString>

struct UserInfo
{
    int id = -1;
    QString phone;
    QString nickname;
    QString avatarPath;
    double balance = 0.0;
    QString registerTime;
    int status = 1;
};

class UserService
{
public:
    UserService();

    bool loginOrRegister(
        const QString &phone,
        const QString &verificationCode,
        const QString &expectedCode,
        UserInfo &userInfo,
        QString &errorMessage
    );

private:
    bool findUserByPhone(
        const QString &phone,
        UserInfo &userInfo,
        QString &errorMessage
    );

    bool createUser(
        const QString &phone,
        UserInfo &userInfo,
        QString &errorMessage
    );
};

#endif // USERSERVICE_H