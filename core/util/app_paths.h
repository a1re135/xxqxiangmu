#pragma once
// ============================================================
// 跨平台路径解析（NFR-C-02）
// 依次在 <当前工作目录>、<可执行文件目录> 下查找相对路径文件，
// 禁止硬编码盘符与反斜杠。
// ============================================================
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QString>
#include <QStringList>

namespace core {

inline QString resolveDataFile(const QString &relativePath)
{
    const QStringList bases = {QDir::currentPath(),
                               QCoreApplication::applicationDirPath()};
    for (const QString &base : bases) {
        const QString path = base + QLatin1Char('/') + relativePath;
        if (QFile::exists(path)) {
            return path;
        }
    }
    return QString();
}

} // namespace core
