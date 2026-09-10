#include "chargerserver.h"

#include "chargerworker.h"

#include <QDebug>
#include <QThread>


ChargerServer::ChargerServer(
    QObject *parent
)
    : QTcpServer(parent)
{
}


void ChargerServer::incomingConnection(
    qintptr socketDescriptor
)
{
    auto *thread =
        new QThread(this);

    auto *worker =
        new ChargerWorker(
            socketDescriptor
        );


    worker->moveToThread(
        thread
    );


    connect(
        thread,
        &QThread::started,
        worker,
        &ChargerWorker::process
    );


    connect(
        worker,
        &ChargerWorker::finished,
        thread,
        &QThread::quit
    );


    connect(
        worker,
        &ChargerWorker::finished,
        worker,
        &QObject::deleteLater
    );


    connect(
        thread,
        &QThread::finished,
        thread,
        &QObject::deleteLater
    );


    thread->start();
}
