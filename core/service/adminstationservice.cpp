#include "adminstationservice.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QRegularExpression>

namespace {

QSqlDatabase connection()
{
    return QSqlDatabase::database(QStringLiteral("ncs_connection"));
}

bool ensureDatabase(QSqlDatabase &db, QString &errorMessage)
{
    db = connection();
    if (!db.isValid() || !db.isOpen()) {
        errorMessage = QStringLiteral("数据库未打开");
        return false;
    }
    return true;
}

bool writeOpsLog(QSqlDatabase &db, const QString &operation, QString &errorMessage)
{
    QSqlQuery log(db);
    log.prepare("INSERT INTO ops_log(operation, created_at) "
                "VALUES(:operation, datetime('now','localtime'))");
    log.bindValue(":operation", operation);
    if (!log.exec()) {
        errorMessage = QStringLiteral("记录运维日志失败：") + log.lastError().text();
        return false;
    }
    return true;
}

QString makeStationAbbreviation(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("ST");
    }

    const QStringList words = trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    QString result;
    if (words.size() >= 2) {
        for (const QString &word : words) {
            if (!word.isEmpty() && word.at(0).isLetterOrNumber()) {
                result += word.at(0).toUpper();
            }
            if (result.size() >= 2) break;
        }
    }

    if (result.size() < 2) {
        for (const QChar ch : trimmed) {
            if (!ch.isSpace() && !ch.isPunct() && !ch.isSymbol()) {
                result += ch;
            }
            if (result.size() >= 2) break;
        }
    }

    if (result.isEmpty()) {
        result = QStringLiteral("ST");
    }
    while (result.size() < 2) result += QChar('T');
    return result.left(4).toUpper();
}

} // namespace

bool AdminStationService::loadStations(QList<StationRecord> &out, QString &errorMessage) const
{
    out.clear();
    errorMessage.clear();

    QSqlDatabase db;
    if (!ensureDatabase(db, errorMessage)) return false;

    QSqlQuery q(db);
    q.prepare(
        "SELECT s.id, s.name, s.address, s.longitude, s.latitude, s.price, "
        "COUNT(c.id) AS total_chargers, "
        "COALESCE(SUM(CASE WHEN c.status != 2 THEN 1 ELSE 0 END), 0) AS non_fault "
        "FROM station s "
        "LEFT JOIN charger c ON c.station_id = s.id "
        "GROUP BY s.id, s.name, s.address, s.longitude, s.latitude, s.price "
        "ORDER BY s.id");

    if (!q.exec()) {
        errorMessage = QStringLiteral("查询电站列表失败：") + q.lastError().text();
        return false;
    }

    while (q.next()) {
        StationRecord s;
        s.id = q.value(0).toInt();
        s.name = q.value(1).toString();
        s.address = q.value(2).toString();
        s.longitude = q.value(3).toDouble();
        s.latitude = q.value(4).toDouble();
        s.price = q.value(5).toDouble();
        s.totalChargers = q.value(6).toInt();
        s.nonFaultChargers = q.value(7).toInt();
        s.onlineRate = s.totalChargers > 0
            ? (static_cast<double>(s.nonFaultChargers) / s.totalChargers * 100.0)
            : 0.0;
        out.append(s);
    }

    return true;
}

bool AdminStationService::loadChargersForStation(int stationId,
                                             QList<ChargerDetail> &out,
                                             QString &errorMessage) const
{
    out.clear();
    errorMessage.clear();
    if (stationId <= 0) {
        errorMessage = QStringLiteral("无效的电站 ID");
        return false;
    }

    QSqlDatabase db;
    if (!ensureDatabase(db, errorMessage)) return false;

    QSqlQuery q(db);
    q.prepare(
        "SELECT id, charger_no, type, power, status, total_count, total_minutes "
        "FROM charger WHERE station_id = :station_id ORDER BY id");
    q.bindValue(":station_id", stationId);

    if (!q.exec()) {
        errorMessage = QStringLiteral("查询站内电桩失败：") + q.lastError().text();
        return false;
    }

    while (q.next()) {
        ChargerDetail c;
        c.id = q.value(0).toInt();
        c.chargerNo = q.value(1).toString();
        c.type = q.value(2).toInt();
        c.power = q.value(3).toDouble();
        c.status = q.value(4).toInt();
        c.totalCount = q.value(5).toInt();
        c.totalMinutes = q.value(6).toInt();
        out.append(c);
    }
    return true;
}

bool AdminStationService::addStation(const QString &name,
                                 const QString &address,
                                 double longitude,
                                 double latitude,
                                 double price,
                                 int initialChargerCount,
                                 double defaultPower,
                                 QString &errorMessage) const
{
    errorMessage.clear();
    if (name.trimmed().isEmpty()) {
        errorMessage = QStringLiteral("请输入电站名称");
        return false;
    }
    if (address.trimmed().isEmpty()) {
        errorMessage = QStringLiteral("请输入详细地址");
        return false;
    }
    if (longitude < -180.0 || longitude > 180.0 || latitude < -90.0 || latitude > 90.0) {
        errorMessage = QStringLiteral("经纬度超出合法范围：经度 -180~180，纬度 -90~90");
        return false;
    }
    if (price < 0.0 || defaultPower <= 0.0 || initialChargerCount < 0) {
        errorMessage = QStringLiteral("价格、功率或初始电桩数量不合法");
        return false;
    }

    QSqlDatabase db;
    if (!ensureDatabase(db, errorMessage)) return false;
    if (!db.transaction()) {
        errorMessage = QStringLiteral("无法开启事务：") + db.lastError().text();
        return false;
    }

    QSqlQuery station(db);
    station.prepare(
        "INSERT INTO station(name, address, longitude, latitude, price) "
        "VALUES(:name, :address, :longitude, :latitude, :price)");
    station.bindValue(":name", name.trimmed());
    station.bindValue(":address", address.trimmed());
    station.bindValue(":longitude", longitude);
    station.bindValue(":latitude", latitude);
    station.bindValue(":price", price);

    if (!station.exec()) {
        errorMessage = QStringLiteral("新增电站失败：") + station.lastError().text();
        db.rollback();
        return false;
    }

    const int stationId = station.lastInsertId().toInt();
    const QString abbreviation = makeStationAbbreviation(name);

    QSqlQuery charger(db);
    charger.prepare(
        "INSERT INTO charger(station_id, charger_no, type, power, status, total_count, total_minutes) "
        "VALUES(:station_id, :charger_no, 0, :power, 0, 0, 0)");

    for (int i = 1; i <= initialChargerCount; ++i) {
        const QString chargerNo = QStringLiteral("%1-%2")
                                      .arg(abbreviation)
                                      .arg(i, 2, 10, QChar('0'));
        charger.bindValue(":station_id", stationId);
        charger.bindValue(":charger_no", chargerNo);
        charger.bindValue(":power", defaultPower);
        if (!charger.exec()) {
            errorMessage = QStringLiteral("批量创建电桩失败：") + charger.lastError().text();
            db.rollback();
            return false;
        }
    }

    if (!writeOpsLog(db,
                     QStringLiteral("新增充电站：%1，初始电桩 %2 台")
                         .arg(name.trimmed())
                         .arg(initialChargerCount),
                     errorMessage)) {
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        errorMessage = QStringLiteral("提交新增电站失败：") + db.lastError().text();
        db.rollback();
        return false;
    }
    return true;
}

bool AdminStationService::updateStation(int stationId,
                                    const QString &name,
                                    const QString &address,
                                    double longitude,
                                    double latitude,
                                    double price,
                                    QString &errorMessage) const
{
    errorMessage.clear();
    if (stationId <= 0) {
        errorMessage = QStringLiteral("无效的电站 ID");
        return false;
    }
    if (name.trimmed().isEmpty()) {
        errorMessage = QStringLiteral("请输入电站名称");
        return false;
    }
    if (address.trimmed().isEmpty()) {
        errorMessage = QStringLiteral("请输入详细地址");
        return false;
    }
    if (longitude < -180.0 || longitude > 180.0 || latitude < -90.0 || latitude > 90.0) {
        errorMessage = QStringLiteral("经纬度超出合法范围：经度 -180~180，纬度 -90~90");
        return false;
    }
    if (price < 0.0) {
        errorMessage = QStringLiteral("单价不能小于 0");
        return false;
    }

    QSqlDatabase db;
    if (!ensureDatabase(db, errorMessage)) return false;
    if (!db.transaction()) {
        errorMessage = QStringLiteral("无法开启事务：") + db.lastError().text();
        return false;
    }

    QSqlQuery q(db);
    q.prepare(
        "UPDATE station SET name=:name, address=:address, longitude=:longitude, "
        "latitude=:latitude, price=:price WHERE id=:id");
    q.bindValue(":name", name.trimmed());
    q.bindValue(":address", address.trimmed());
    q.bindValue(":longitude", longitude);
    q.bindValue(":latitude", latitude);
    q.bindValue(":price", price);
    q.bindValue(":id", stationId);

    if (!q.exec() || q.numRowsAffected() != 1) {
        errorMessage = QStringLiteral("修改电站失败：") + q.lastError().text();
        db.rollback();
        return false;
    }

    if (!writeOpsLog(db, QStringLiteral("修改充电站：#%1 %2").arg(stationId).arg(name.trimmed()), errorMessage)) {
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        errorMessage = QStringLiteral("提交修改电站失败：") + db.lastError().text();
        db.rollback();
        return false;
    }
    return true;
}

bool AdminStationService::deleteStation(int stationId, QString &errorMessage) const
{
    errorMessage.clear();
    if (stationId <= 0) {
        errorMessage = QStringLiteral("无效的电站 ID");
        return false;
    }

    QSqlDatabase db;
    if (!ensureDatabase(db, errorMessage)) return false;
    if (!db.transaction()) {
        errorMessage = QStringLiteral("无法开启事务：") + db.lastError().text();
        return false;
    }

    QSqlQuery check(db);
    check.prepare("SELECT name FROM station WHERE id=:id");
    check.bindValue(":id", stationId);
    if (!check.exec() || !check.next()) {
        errorMessage = QStringLiteral("找不到指定电站");
        db.rollback();
        return false;
    }
    const QString name = check.value(0).toString();

    QSqlQuery count(db);
    count.prepare("SELECT COUNT(*) FROM charger WHERE station_id=:id");
    count.bindValue(":id", stationId);
    if (!count.exec() || !count.next()) {
        errorMessage = QStringLiteral("检查站内电桩失败：") + count.lastError().text();
        db.rollback();
        return false;
    }
    const int chargerCount = count.value(0).toInt();
    if (chargerCount > 0) {
        errorMessage = QStringLiteral("该电站下仍有 %1 个电桩，禁止删除。请先删除电桩后再删除电站。")
                           .arg(chargerCount);
        db.rollback();
        return false;
    }

    QSqlQuery q(db);
    q.prepare("DELETE FROM station WHERE id=:id");
    q.bindValue(":id", stationId);
    if (!q.exec() || q.numRowsAffected() != 1) {
        errorMessage = QStringLiteral("删除电站失败：") + q.lastError().text();
        db.rollback();
        return false;
    }

    if (!writeOpsLog(db, QStringLiteral("删除充电站：#%1 %2").arg(stationId).arg(name), errorMessage)) {
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        errorMessage = QStringLiteral("提交删除电站失败：") + db.lastError().text();
        db.rollback();
        return false;
    }
    return true;
}
