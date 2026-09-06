#include "ncslogger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QStandardPaths>
#include <QTextStream>
#include <QtGlobal>

namespace {
QMutex g_logMutex;


void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    Q_UNUSED(context);
    QMutexLocker locker(&g_logMutex);

    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(base);
    if (!dir.exists("logs")) {
        dir.mkpath("logs");
    }

    const QString fileName = QStringLiteral("logs/ncs_%1.log")
        .arg(QDate::currentDate().toString(QStringLiteral("yyyyMMdd")));
    QFile file(dir.filePath(fileName));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }

    QString level;
    switch (type) {
    case QtDebugMsg: level = QStringLiteral("DEBUG"); break;
    case QtInfoMsg: level = QStringLiteral("INFO"); break;
    case QtWarningMsg: level = QStringLiteral("WARN"); break;
    case QtCriticalMsg: level = QStringLiteral("ERROR"); break;
    case QtFatalMsg: level = QStringLiteral("FATAL"); break;
    }

    QTextStream out(&file);
    out << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
        << " [" << level << "] ["
        << (context.category && *context.category ? context.category : "default")
        << "] " << message << Qt::endl;
}
}

void NcsLogger::install()
{
    qInstallMessageHandler(messageHandler);
    qInfo() << "NCS logger initialized";
}
