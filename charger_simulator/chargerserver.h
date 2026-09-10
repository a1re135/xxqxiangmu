#ifndef CHARGERSERVER_H
#define CHARGERSERVER_H

#include <QTcpServer>

class ChargerServer : public QTcpServer
{
    Q_OBJECT

public:
    explicit ChargerServer(
        QObject *parent = nullptr
    );

protected:
    void incomingConnection(
        qintptr socketDescriptor
    ) override;
};

#endif // CHARGERSERVER_H
