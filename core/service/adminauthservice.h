#ifndef ADMINAUTHSERVICE_H
#define ADMINAUTHSERVICE_H

#include <QString>

class AdminAuthService
{
public:
    struct AdminInfo
    {
        int id = -1;
        QString username;
    };

    bool authenticate(
        const QString &username,
        const QString &password,
        AdminInfo &adminInfo,
        QString &errorMessage
    ) const;

private:
    bool verifyPassword(
        const QString &password,
        const QString &storedHash,
        const QString &salt
    ) const;
};

#endif // ADMINAUTHSERVICE_H
