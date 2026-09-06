#include "station_repository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include "../util/logger.h"

namespace core {

namespace {
const char *kModule = "StationRepository";
} // namespace

StationRepository::StationRepository(QSqlDatabase db)
    : m_db(db)
{
}

QVector<Station> StationRepository::loadAllStations() const
{
    QVector<Station> result;
    QSqlQuery q(m_db);
    // 常量 SQL，无用户输入拼接（NFR-M-03）
    q.prepare(QStringLiteral(
        "SELECT id, name, address, latitude, longitude, price FROM station ORDER BY id"));
    if (!q.exec()) {
        LOG_ERROR(kModule, QStringLiteral("查询电站失败: %1").arg(q.lastError().text()));
        return result;
    }
    while (q.next()) {
        Station s;
        s.id = q.value(0).toInt();
        s.name = q.value(1).toString();
        s.address = q.value(2).toString();
        s.latitude = q.value(3).toDouble();
        s.longitude = q.value(4).toDouble();
        s.price = q.value(5).toDouble();
        result.append(s);
    }
    return result;
}

QVector<StationCardData> StationRepository::loadStationCards() const
{
    QVector<StationCardData> result;
    // LEFT JOIN + GROUP BY 一次取回电站与实时电桩统计：
    // 空闲数 = status=0 计数；总桩数 = COUNT(charger.id)
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT s.id, s.name, s.address, s.latitude, s.longitude, s.price, "
        "       COUNT(c.id) AS total_count, "
        "       COALESCE(SUM(CASE WHEN c.status = 0 THEN 1 ELSE 0 END), 0) AS free_count "
        "FROM station s "
        "LEFT JOIN charger c ON c.station_id = s.id "
        "GROUP BY s.id, s.name, s.address, s.latitude, s.longitude, s.price "
        "ORDER BY s.id"));
    if (!q.exec()) {
        LOG_ERROR(kModule, QStringLiteral("查询电站卡片数据失败: %1").arg(q.lastError().text()));
        return result;
    }
    while (q.next()) {
        StationCardData card;
        card.station.id = q.value(0).toInt();
        card.station.name = q.value(1).toString();
        card.station.address = q.value(2).toString();
        card.station.latitude = q.value(3).toDouble();
        card.station.longitude = q.value(4).toDouble();
        card.station.price = q.value(5).toDouble();
        card.totalChargers = q.value(6).toInt();
        card.freeChargers = q.value(7).toInt();
        result.append(card);
    }
    return result;
}

int StationRepository::countFreeChargers(int stationId) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM charger WHERE station_id = ? AND status = 0"));
    q.addBindValue(stationId);
    if (!q.exec() || !q.next()) {
        LOG_ERROR(kModule,
                  QStringLiteral("统计空闲桩数失败(电站%1): %2")
                      .arg(stationId)
                      .arg(q.lastError().text()));
        return 0;
    }
    return q.value(0).toInt();
}

bool StationRepository::loadStationById(int id, Station *out) const
{
    if (!out) {
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, name, address, latitude, longitude, price FROM station WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec() || !q.next()) {
        LOG_WARN(kModule, QStringLiteral("电站不存在或查询失败: id=%1").arg(id));
        return false;
    }
    out->id = q.value(0).toInt();
    out->name = q.value(1).toString();
    out->address = q.value(2).toString();
    out->latitude = q.value(3).toDouble();
    out->longitude = q.value(4).toDouble();
    out->price = q.value(5).toDouble();
    return true;
}

} // namespace core
