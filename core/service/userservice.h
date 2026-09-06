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
    bool getUserById(
            int id,
            UserInfo &userInfo,
            QString &errorMessage
        );


        // 修改头像
    bool updateAvatar(
        int id,
        const QString &avatarPath,
        QString &errorMessage
    );


        // 修改昵称
    bool updateNickname(
        int id,
        const QString &nickname,
        QString &errorMessage
    );


        // 用户充值
    bool recharge(
        int id,
        double amount,
        double &newBalance,
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
