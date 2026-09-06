#pragma once
// ============================================================
// 统一日志模块（NFR-M-04）
// 输出到 logs/ncs_YYYYMMDD.log，含级别、时间、模块信息。
// ============================================================
#include <QFile>
#include <QMutex>
#include <QString>

namespace core {

class Logger {
public:
    enum class Level { Debug, Info, Warn, Error };

    static Logger &instance();

    // 日志目录（默认 <当前工作目录>/logs）
    void setLogDir(const QString &dir);
    QString logDir() const { return m_dir; }

    void write(Level level, const QString &module, const QString &message);
    static QString levelName(Level level);

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    void ensureFile();

    QString m_dir;
    QFile m_file;
    QMutex m_mutex;
};

} // namespace core

#define LOG_DEBUG(module, msg) \
    ::core::Logger::instance().write(::core::Logger::Level::Debug, module, msg)
#define LOG_INFO(module, msg) \
    ::core::Logger::instance().write(::core::Logger::Level::Info, module, msg)
#define LOG_WARN(module, msg) \
    ::core::Logger::instance().write(::core::Logger::Level::Warn, module, msg)
#define LOG_ERROR(module, msg) \
    ::core::Logger::instance().write(::core::Logger::Level::Error, module, msg)
