#ifndef CHARGERCOMMANDCLIENT_H
#define CHARGERCOMMANDCLIENT_H

#include <QObject>
#include <QString>

class QTcpSocket;
class QTimer;

class ChargerCommandClient : public QObject
{
    Q_OBJECT

public:
    explicit ChargerCommandClient(
        QObject *parent = nullptr
    );

    void restartCharger(
        int chargerId,
        const QString &host =
            QStringLiteral("127.0.0.1"),
        quint16 port = 45454
    );

signals:
    void restartFinished(
        int chargerId,
        bool success,
        const QString &message
    );

private:
    void handleConnected();
    void handleReadyRead();
    void finish(
        bool success,
        const QString &message
    );

    QTcpSocket *m_socket = nullptr;
    QTimer *m_timeoutTimer = nullptr;

    int m_chargerId = -1;
    QByteArray m_buffer;

    bool m_finished = false;
};

#endif // CHARGERCOMMANDCLIENT_H
