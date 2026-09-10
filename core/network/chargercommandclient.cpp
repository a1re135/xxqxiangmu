#include "chargercommandclient.h"

#include <QAbstractSocket>
#include <QDebug>
#include <QTcpSocket>
#include <QTimer>


ChargerCommandClient::ChargerCommandClient(
    QObject *parent
)
    : QObject(parent)
{
    m_socket =
        new QTcpSocket(this);

    m_timeoutTimer =
        new QTimer(this);

    m_timeoutTimer->setSingleShot(true);

    m_timeoutTimer->setInterval(
        6000
    );


    connect(
        m_socket,
        &QTcpSocket::connected,
        this,
        &ChargerCommandClient::handleConnected
    );


    connect(
        m_socket,
        &QTcpSocket::readyRead,
        this,
        &ChargerCommandClient::handleReadyRead
    );


    connect(
        m_socket,
        &QTcpSocket::errorOccurred,
        this,
        [this](
            QAbstractSocket::SocketError)
        {
            if (m_finished) {
                return;
            }

            finish(
                false,
                QStringLiteral(
                    "Socket 通信失败：%1"
                ).arg(
                    m_socket->errorString()
                )
            );
        }
    );


    connect(
        m_timeoutTimer,
        &QTimer::timeout,
        this,
        [this]()
        {
            finish(
                false,
                QStringLiteral(
                    "等待电桩响应超时。"
                )
            );
        }
    );
}


void ChargerCommandClient::restartCharger(
    int chargerId,
    const QString &host,
    quint16 port
)
{
    if (chargerId <= 0) {

        emit restartFinished(
            chargerId,
            false,
            QStringLiteral(
                "无效的电桩编号。"
            )
        );

        return;
    }


    if (m_socket->state()
        != QAbstractSocket::UnconnectedState) {

        m_socket->abort();
    }


    m_chargerId = chargerId;

    m_buffer.clear();

    m_finished = false;


    qInfo()
        << "[Socket Client]"
        << "Connecting to"
        << host
        << port;


    m_timeoutTimer->start();


    m_socket->connectToHost(
        host,
        port
    );
}


void ChargerCommandClient::handleConnected()
{
    const QByteArray command =
        QStringLiteral(
            "RESTART %1\n"
        )
            .arg(m_chargerId)
            .toUtf8();


    qInfo()
        << "[Socket Client]"
        << "Sending:"
        << command.trimmed();


    m_socket->write(
        command
    );

    m_socket->flush();
}


void ChargerCommandClient::handleReadyRead()
{
    m_buffer.append(
        m_socket->readAll()
    );


    // Wait until a complete line arrives.
    if (!m_buffer.contains('\n')) {
        return;
    }


    const QString response =
        QString::fromUtf8(
            m_buffer
        )
            .trimmed();


    qInfo()
        << "[Socket Client]"
        << "Received:"
        << response;


    const QString expected =
        QStringLiteral(
            "OK %1"
        ).arg(
            m_chargerId
        );


    if (response == expected) {

        finish(
            true,
            QStringLiteral(
                "电桩已确认重启指令。"
            )
        );

        return;
    }


    finish(
        false,
        QStringLiteral(
            "电桩返回错误：%1"
        ).arg(
            response
        )
    );
}


void ChargerCommandClient::finish(
    bool success,
    const QString &message
)
{
    if (m_finished) {
        return;
    }

    m_finished = true;

    m_timeoutTimer->stop();


    if (m_socket->state()
        != QAbstractSocket::UnconnectedState) {

        m_socket->disconnectFromHost();
    }


    emit restartFinished(
        m_chargerId,
        success,
        message
    );
}
