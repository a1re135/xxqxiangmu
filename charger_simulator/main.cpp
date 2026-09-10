#include "chargerserver.h"

#include <QCoreApplication>
#include <QDebug>
#include <QHostAddress>


int main(
    int argc,
    char *argv[]
)
{
    QCoreApplication app(
        argc,
        argv
    );


    ChargerServer server;


    constexpr quint16 port =
        45454;


    if (!server.listen(
            QHostAddress::LocalHost,
            port)) {

        qCritical()
            << "Unable to start charger server:"
            << server.errorString();

        return -1;
    }


    qInfo()
        << "====================================";

    qInfo()
        << "NCS Charger Simulator";

    qInfo()
        << "Listening on"
        << server.serverAddress()
        << ":"
        << server.serverPort();

    qInfo()
        << "Socket communication: ENABLED";

    qInfo()
        << "Multithreading: ENABLED";

    qInfo()
        << "====================================";


    return app.exec();
}
