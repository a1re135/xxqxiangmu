#pragma once
// ============================================================
// 电站数据存取（UC-U-02/03、UC-A-06 复用）
// 所有 SQL 仅存在于 core/database 层（NFR-M-02），
// 所有查询使用参数化绑定（NFR-M-03）。
// ============================================================
#include <QSqlDatabase>
#include <QString>
#include <QVector>

namespace core {

struct Station {
    int id = 0;
    QString name;
    QString address;
    double latitude = 0.0;
    double longitude = 0.0;
    double price = 0.0; // 单价：元/度
};

// 电站列表卡片所需聚合数据（总桩数 + 空闲桩数，实时统计自 charger 表 status=0）
struct StationCardData {
    Station station;
    int totalChargers = 0;
    int freeChargers = 0;
};

class StationRepository {
public:
    explicit StationRepository(QSqlDatabase db);

    // 读取全部电站（基础信息）
    QVector<Station> loadAllStations() const;

    // 一次 JOIN 聚合返回全部电站卡片数据（含总桩数/空闲桩数）
    QVector<StationCardData> loadStationCards() const;

    // 单站空闲桩数（status=0）
    int countFreeChargers(int stationId) const;

    bool loadStationById(int id, Station *out) const;

private:
    QSqlDatabase m_db;
};

} // namespace core
