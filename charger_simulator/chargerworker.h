#ifndef CHARGERWORKER_H
#define CHARGERWORKER_H

#include <QObject>
#include <QTcpSocket>

class ChargerWorker : public QObject
{
    Q_OBJECT

public:
    explicit ChargerWorker(
        qintptr socketDescriptor,
        QObject *parent = nullptr
    );

public slots:
    void process();

signals:
    void finished();

private:
    qintptr m_socketDescriptor = -1;
};

#endif // CHARGERWORKER_H
