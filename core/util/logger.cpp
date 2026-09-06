#include "logger.h"

#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDir>

namespace core {

Logger &Logger::instance()
{
    static Logger inst;
    return inst;
}

Logger::~Logger()
{
    if (m_file.isOpen()) {
        m_file.flush();
        m_file.close();
    }
}

void Logger::setLogDir(const QString &dir)
{
    QMutexLocker locker(&m_mutex);
    m_dir = dir;
    ensureFile();
}

QString Logger::levelName(Level level)
{
    switch (level) {
    case Level::Debug: return "DEBUG";
    case Level::Info:  return "INFO";
    case Level::Warn:  return "WARN";
    case Level::Error: return "ERROR";
    }
    return "UNKNOWN";
}

void Logger::ensureFile()
{
    if (m_dir.isEmpty()) {
        m_dir = QDir::currentPath() + QStringLiteral("/logs");
    }
    if (m_file.isOpen()) {
        return;
    }
    QDir().mkpath(m_dir);
    const QString path = m_dir + QStringLiteral("/ncs_")
                         + QDate::currentDate().toString(QStringLiteral("yyyyMMdd"))
                         + QStringLiteral(".log");
    m_file.setFileName(path);
    m_file.open(QIODevice::Append | QIODevice::Text);
}

void Logger::write(Level level, const QString &module, const QString &message)
{
    QMutexLocker locker(&m_mutex);
    ensureFile();
    const QString line = QStringLiteral("%1 [%2] [%3] %4")
                             .arg(QDateTime::currentDateTime().toString(
                                      QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
                                  levelName(level), module, message);
    if (m_file.isOpen()) {
        m_file.write((line + QLatin1Char('\n')).toUtf8());
        m_file.flush();
    }
    qDebug().noquote() << line;
}

} // namespace core
