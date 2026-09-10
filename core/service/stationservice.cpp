#include "stationservice.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <algorithm>
#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>

#include "../util/app_paths.h"
#include "../util/geo_util.h"
#include "../util/logger.h"

namespace core {

namespace {
const char *kModule = "StationService";
} // namespace

StationService::StationService(DatabaseManager *db, QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_repo(m_db ? m_db->connection() : QSqlDatabase())
    , m_regions({
          // 区域下拉框预置项（北京市各区，演示定位）
          {"北京市·房山区·良乡", "北京", 39.7313, 116.1432},
          {"北京市·海淀区", "北京", 39.9593, 116.2981},
          {"北京市·朝阳区", "北京", 39.9219, 116.4435},
          {"北京市·丰台区", "北京", 39.8585, 116.2870},
          {"北京市·西城区", "北京", 39.9151, 116.3660},
          {"北京市·东城区", "北京", 39.9175, 116.4180},
          {"北京市·昌平区", "北京", 40.2208, 116.2312},
          {"北京市·通州区", "北京", 39.9097, 116.6564},
      })
{
    connect(&m_geocoder, &Geocoder::finished, this,
            &StationService::onGeocodeFinished);
}

QStringList StationService::presetRegionNames() const
{
    QStringList names;
    for (const PresetRegion &r : m_regions) {
        names.append(r.name);
    }
    return names;
}

bool StationService::presetRegionByName(const QString &name, PresetRegion *out) const
{
    for (const PresetRegion &r : m_regions) {
        if (r.name == name) {
            if (out) {
                *out = r;
            }
            return true;
        }
    }
    return false;
}

QString StationService::defaultRegionName() const
{
    return m_regions.isEmpty() ? QString() : m_regions.first().name;
}

void StationService::locate(const QString &regionName, const QString &address)
{
    const QString addr = address.trimmed();
    // 未输入地址 → 直接用区域预置坐标
    if (addr.isEmpty()) {
        emitPresetLocation(regionName, addr);
        return;
    }
    // 未配置腾讯地图 Key → 退化为预置坐标（不发起网络请求）
    const QString key = loadTencentKey();
    if (key.isEmpty()) {
        emitPresetLocation(regionName, addr,
                           QStringLiteral("未配置腾讯地图Key，已使用区域预置位置"));
        return;
    }
    // 已配置 Key → 调用地理编码接口（含超时），失败回退预置坐标
    m_pendingRegion = regionName;
    m_pendingAddress = addr;
    m_geocoder.geocode(addr, key);
}

bool StationService::loadNextPrediction(
    int stationId,
    const QString &generatedTime,
    double &predictedLoad,
    int &predictedFreeChargers,
    bool &isPeak
) const
{
    predictedLoad = 0.0;
    predictedFreeChargers = -1;
    isPeak = false;

    if (!m_db
        || generatedTime.isEmpty()) {
        return false;
    }

    const QSqlDatabase db =
        m_db->connection();

    if (!db.isOpen()) {
        return false;
    }

    QSqlQuery query(db);

    query.prepare(
        "SELECT "
        "predicted_load, "
        "predicted_free_chargers, "
        "is_peak "
        "FROM load_prediction "
        "WHERE station_id = :station_id "
        "AND generated_time = :generated_time "
        "AND target_time >= :now "
        "ORDER BY target_time ASC "
        "LIMIT 1"
    );

    query.bindValue(
        ":station_id",
        stationId
    );

    query.bindValue(
        ":generated_time",
        generatedTime
    );

    query.bindValue(
        ":now",
        QDateTime::currentDateTime()
            .toString(
                QStringLiteral(
                    "yyyy-MM-dd HH:mm:ss"
                )
            )
    );

    if (!query.exec()) {
        LOG_WARN(
            kModule,
            QStringLiteral(
                "读取电站 %1 预测失败：%2"
            )
                .arg(stationId)
                .arg(
                    query.lastError().text()
                )
        );

        return false;
    }

    if (!query.next()) {
        return false;
    }

    predictedLoad =
        qMax(
            0.0,
            query.value(0).toDouble()
        );

    predictedFreeChargers =
        query.value(1).toInt();

    isPeak =
        query.value(2).toInt() != 0;

    return true;
}

QVector<StationListItem>
StationService::listStationsByDistance(double latitude, double longitude) const
{
    QVector<StationListItem> items;
    const QVector<StationCardData> cards = m_repo.loadStationCards();
    items.reserve(cards.size());
    for (const StationCardData &card : cards) {
        StationListItem item;
        item.id = card.station.id;
        item.name = card.station.name;
        item.address = card.station.address;
        item.price = card.station.price;
        item.latitude = card.station.latitude;
        item.longitude = card.station.longitude;
        item.totalChargers = card.totalChargers;
        item.freeChargers = card.freeChargers;
        item.distanceKm = GeoUtil::haversineKm(latitude, longitude,
                                               card.station.latitude,
                                               card.station.longitude);
        items.append(item);
    }
    // 按距离升序
    std::sort(items.begin(), items.end(),
              [](const StationListItem &a, const StationListItem &b) {
                  return a.distanceKm < b.distanceKm;
              });
    return items;
}

QVector<StationListItem>
StationService::listStationsSmartRecommended(
    double latitude,
    double longitude
) const
{
    QVector<StationListItem> items =
        listStationsByDistance(
            latitude,
            longitude
        );


    if (items.isEmpty()) {
        return items;
    }


    bool anyPrediction = false;

    const QString predictionBatch =
        latestPredictionBatch();

    for (StationListItem &item : items) {

        double predictedLoad = 0.0;
        int predictedFree = -1;
        bool predictedPeak = false;


        const bool hasPrediction =
            loadNextPrediction(
                item.id,
                predictionBatch,
                predictedLoad,
                predictedFree,
                predictedPeak
            );


        item.hasPrediction =
            hasPrediction;


        if (hasPrediction) {

            anyPrediction = true;

            item.predictedLoad =
                qMax(
                    0.0,
                    predictedLoad
                );


            item.predictedFreeChargers =
                qBound(
                    0,
                    predictedFree,
                    item.totalChargers
                );


            item.predictedPeak =
                predictedPeak;
        }


        // If ML data is unavailable for this station,
        // use current availability as fallback.
        const int availableChargers =
            hasPrediction
                ? item.predictedFreeChargers
                : item.freeChargers;


        const double availabilityScore =
            item.totalChargers > 0
                ? static_cast<double>(
                      availableChargers
                  )
                    / item.totalChargers
                : 0.0;


        // Nearer station = larger score.
        const double distanceScore =
            1.0
            / (
                1.0
                + qMax(
                    0.0,
                    item.distanceKm
                )
            );


        // Prediction availability matters more
        // than distance.
        item.recommendationScore =
            0.75 * availabilityScore
            + 0.25 * distanceScore;


        // Penalize predicted peak periods.
        if (hasPrediction
            && predictedPeak) {

            item.recommendationScore *=
                0.85;
        }
    }


    // If there is no ML data at all,
    // preserve the normal distance order.
    if (!anyPrediction) {
        return items;
    }


    std::sort(
        items.begin(),
        items.end(),
        [](
            const StationListItem &a,
            const StationListItem &b)
        {
            const double difference =
                a.recommendationScore
                - b.recommendationScore;


            if (qAbs(difference)
                > 0.000001) {

                return
                    a.recommendationScore
                    > b.recommendationScore;
            }


            return
                a.distanceKm
                < b.distanceKm;
        }
    );


    if (!items.isEmpty()) {
        items[0].recommended = true;
    }


    return items;
}

QString StationService::loadTencentKey()
{
    // 1) 环境变量优先（便于测试与部署注入）
    const QByteArray env = qgetenv("NCS_TENCENT_KEY");
    if (!env.isEmpty()) {
        return QString::fromUtf8(env).trimmed();
    }
    // 2) config/app.ini：<当前目录> 或 <可执行文件目录>
    const QString iniPath = resolveDataFile(QStringLiteral("config/app.ini"));
    if (!iniPath.isEmpty()) {
        QSettings settings(iniPath, QSettings::IniFormat);
        const QString key =
            settings.value(QStringLiteral("map/tencent_key")).toString().trimmed();
        if (!key.isEmpty()) {
            return key;
        }
    }
    LOG_INFO(kModule, QStringLiteral("未配置腾讯地图Key，定位将使用区域预置坐标"));
    return QString();
}

void StationService::onGeocodeFinished(bool ok, double latitude, double longitude,
                                       const QString &errorMessage)
{
    Q_UNUSED(errorMessage);
    if (ok) {
        m_lat = latitude;
        m_lng = longitude;
        m_regionName = m_pendingRegion;
        LocationResult r;
        r.ok = true;
        r.regionName = m_pendingRegion;
        r.latitude = latitude;
        r.longitude = longitude;
        r.usedFallback = false;
        r.message = QStringLiteral("定位成功：%1（(%2, %3)）")
                        .arg(m_pendingAddress)
                        .arg(latitude, 0, 'f', 4)
                        .arg(longitude, 0, 'f', 4);
        LOG_INFO(kModule, r.message);
        emit located(r);
    } else {
        // 地址解析失败或网络超时 → 退化为预置坐标（UC-U-02 异常流 E1）
        LOG_WARN(kModule,
                 QStringLiteral("地址解析失败，已使用默认位置（区域：%1）")
                     .arg(m_pendingRegion));
        emitPresetLocation(m_pendingRegion, m_pendingAddress,
                           QStringLiteral("地址解析失败，已使用默认位置"));
    }
}

void StationService::emitPresetLocation(const QString &regionName,
                                        const QString &address,
                                        const QString &extraMessage)
{
    PresetRegion region;
    if (!presetRegionByName(regionName, &region)) {
        LocationResult r;
        r.ok = false;
        r.message = QStringLiteral("未知区域：%1").arg(regionName);
        LOG_ERROR(kModule, r.message);
        emit located(r);
        return;
    }
    m_lat = region.latitude;
    m_lng = region.longitude;
    m_regionName = region.name;
    LocationResult r;
    r.ok = true;
    r.regionName = region.name;
    r.latitude = region.latitude;
    r.longitude = region.longitude;
    r.usedFallback = !extraMessage.isEmpty();
    r.message = extraMessage.isEmpty()
                    ? QStringLiteral("定位成功：%1（预置坐标）").arg(region.name)
                    : extraMessage;
    Q_UNUSED(address);
    LOG_INFO(kModule, QStringLiteral("%1 → (%2, %3)")
                          .arg(r.message)
                          .arg(r.latitude, 0, 'f', 4)
                          .arg(r.longitude, 0, 'f', 4));
    emit located(r);
}

bool StationService::getChargersByStationId(
    int stationId,
    QVector<ChargerData> &chargers,
    QString *errorMessage
) const
{
    return m_repo.loadChargersByStationId(
        stationId,
        chargers,
        errorMessage
    );
}

QString StationService::latestPredictionBatch() const
{
    if (!m_db) {
        return {};
    }

    const QSqlDatabase db =
        m_db->connection();

    if (!db.isOpen()) {
        return {};
    }

    QSqlQuery query(db);

    query.prepare(
        "SELECT generated_time "
        "FROM load_prediction "
        "WHERE target_time >= :now "
        "GROUP BY generated_time "
        "ORDER BY generated_time DESC "
        "LIMIT 1"
    );

    query.bindValue(
        ":now",
        QDateTime::currentDateTime()
            .toString(
                QStringLiteral(
                    "yyyy-MM-dd HH:mm:ss"
                )
            )
    );

    if (!query.exec()) {
        LOG_WARN(
            kModule,
            QStringLiteral(
                "读取最新预测批次失败：%1"
            ).arg(
                query.lastError().text()
            )
        );

        return {};
    }

    if (!query.next()) {
        return {};
    }

    return query.value(0).toString();
}

} // namespace core
