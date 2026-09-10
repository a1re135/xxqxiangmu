#include "chargerworker.h"

#include <QDebug>
#include <QRegularExpression>
#include <QThread>

ChargerWorker::ChargerWorker(
    qintptr socketDescriptor,
    QObject *parent
)
    : QObject(parent)
    , m_socketDescriptor(socketDescriptor)
{
}


void ChargerWorker::process()
{
    QTcpSocket socket;

    if (!socket.setSocketDescriptor(
            m_socketDescriptor)) {

        qWarning()
            << "Unable to attach socket:"
            << socket.errorString();

        emit finished();
        return;
    }


    qInfo()
        << "[Socket] Client connected."
        << "Thread:"
        << QThread::currentThreadId();


    if (!socket.waitForReadyRead(3000)) {

        qWarning()
            << "[Socket] No command received.";

        socket.disconnectFromHost();

        emit finished();
        return;
    }


    const QString command =
        QString::fromUtf8(
            socket.readAll()
        )
            .trimmed();


    qInfo()
        << "[Socket] Received:"
        << command;


    // Expected:
    //
    // RESTART 12

    static const QRegularExpression
        restartPattern(
            QStringLiteral(
                "^RESTART\\s+(\\d+)$"
            )
        );


    const QRegularExpressionMatch match =
        restartPattern.match(command);


    QByteArray response;


    if (match.hasMatch()) {

        const int chargerId =
            match.captured(1).toInt();


        qInfo()
            << "[Worker]"
            << "Restarting charger"
            << chargerId
            << "on thread"
            << QThread::currentThreadId();


        // Simulate the charging pile rebooting.
        QThread::msleep(2000);


        response =
            QStringLiteral(
                "OK %1\n"
            )
                .arg(chargerId)
                .toUtf8();

    } else {

        response =
            QByteArrayLiteral(
                "ERROR INVALID_COMMAND\n"
            );
    }


    socket.write(response);

    socket.waitForBytesWritten(1000);

    socket.disconnectFromHost();

    socket.waitForDisconnected(500);


    qInfo()
        << "[Socket] Finished command:"
        << command;


    emit finished();
}
