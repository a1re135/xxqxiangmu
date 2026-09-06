#pragma once
// ============================================================
// 电站查询服务（UC-U-02 附近充电站查询）
// 职责：
//   1. 定位：区域预置经纬度（本地）/ 腾讯地图地理编码（配置 Key 且网络可用）
//   2. Haversine 距离计算 + 按距离升序排序
//   3. 提供电站列表卡片数据（空闲数实时统计自 charger 表 status=0）
// UI 层不出现任何 SQL（NFR-M-02），本层是用户端电站功能唯一入口。
// ============================================================
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "../database/databasemanager.h"
#include "../database/station_repository.h"
#include "geocoder.h"

namespace core {

// 区域下拉框预置项：城市/城区 + 经纬度
struct PresetRegion {
    QString name;
    QString city;
    double latitude = 0.0;
    double longitude = 0.0;
};

// 定位结果
struct LocationResult {
    bool ok = true;
    QString message;     // 界面提示文案
    QString regionName;  // 生效的区域名
    double latitude = 0.0;
    double longitude = 0.0;
    bool usedFallback = false; // true=地址解析失败退化为预置坐标
};

// 电站列表卡片数据（含距离）
struct StationListItem {
    int id = 0;
    QString name;
    QString address;
    double price = 0.0; // 元/度
    int totalChargers = 0;
    int freeChargers = 0; // 实时统计 status=0
    double distanceKm = 0.0;
};

class StationService : public QObject {
    Q_OBJECT
public:
    explicit StationService(DatabaseManager *db, QObject *parent = nullptr);

    // 区域下拉框名称列表（按此顺序展示）
    QStringList presetRegionNames() const;
    bool presetRegionByName(const QString &name, PresetRegion *out) const;
    // 默认（首项）区域名
    QString defaultRegionName() const;

    // 定位入口：优先走地理编码（地址非空且配置了 Key），否则用区域预置坐标。
    // 结果通过 located 信号异步返回（预置路径也会经事件循环发出，保证 UI 一致）。
    void locate(const QString &regionName, const QString &address);

    // 按当前位置与全部电站的 Haversine 距离升序返回列表
    QVector<StationListItem> listStationsByDistance(double latitude,
                                                   double longitude) const;

    double lastLatitude() const { return m_lat; }
    double lastLongitude() const { return m_lng; }
    QString lastRegionName() const { return m_regionName; }

    // 腾讯地图 Key：环境变量 NCS_TENCENT_KEY > config/app.ini [map] tencent_key
    static QString loadTencentKey();

signals:
    void located(const LocationResult &result);

private slots:
    void onGeocodeFinished(bool ok, double latitude, double longitude,
                           const QString &errorMessage);

private:
    void emitPresetLocation(const QString &regionName, const QString &address,
                            const QString &extraMessage = QString());

    DatabaseManager *m_db = nullptr;
    StationRepository m_repo;
    Geocoder m_geocoder;
    QVector<PresetRegion> m_regions;

    // 最近一次生效位置（供刷新/二次进入使用）
    double m_lat = 0.0;
    double m_lng = 0.0;
    QString m_regionName;

    // 地理编码在途请求的上下文
    QString m_pendingRegion;
    QString m_pendingAddress;
};

} // namespace core
