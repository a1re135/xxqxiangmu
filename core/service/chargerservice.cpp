#include "chargerservice.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

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
    log.prepare(
        "INSERT INTO ops_log(operation, created_at) "
        "VALUES(:operation, datetime('now','localtime'))");
    log.bindValue(":operation", operation);
    if (!log.exec()) {
        errorMessage = QStringLiteral("记录运维日志失败：") + log.lastError().text();
        return false;
    }
    return true;
}

} // namespace

bool ChargerService::loadChargers(
    QList<ChargerRecord> &out,
    QString &errorMessage) const
{
    out.clear();
    errorMessage.clear();

    QSqlDatabase db;
    if (!ensureDatabase(db, errorMessage)) {
        return false;
    }

    QSqlQuery query(db);
    query.prepare(
        "SELECT c.id, c.station_id, c.charger_no, s.name, "
        "c.type, c.power, c.status, c.total_count, c.total_minutes "
        "FROM charger c "
        "INNER JOIN station s ON s.id = c.station_id "
        "ORDER BY c.id");

    if (!query.exec()) {
        errorMessage = QStringLiteral("查询电桩列表失败：") + query.lastError().text();
        return false;
    }

    while (query.next()) {
        ChargerRecord r;
        r.id = query.value(0).toInt();
        r.stationId = query.value(1).toInt();
        r.chargerNo = query.value(2).toString();
        r.stationName = query.value(3).toString();
        r.type = query.value(4).toInt();
        r.power = query.value(5).toDouble();
        r.status = query.value(6).toInt();
        r.totalCount = query.value(7).toInt();
        r.totalMinutes = query.value(8).toInt();
        out.append(r);
    }

    return true;
}

bool ChargerService::loadStations(
    QList<StationOption> &out,
    QString &errorMessage) const
{
    out.clear();
    errorMessage.clear();

    QSqlDatabase db;
    if (!ensureDatabase(db, errorMessage)) {
        return false;
    }

    QSqlQuery query(db);
    query.prepare("SELECT id, name FROM station ORDER BY id");
    if (!query.exec()) {
        errorMessage = QStringLiteral("查询电站列表失败：") + query.lastError().text();
        return false;
    }

    while (query.next()) {
        StationOption s;
        s.id = query.value(0).toInt();
        s.name = query.value(1).toString();
        out.append(s);
    }
    return true;
}

bool ChargerService::setFault(int chargerId, QString &errorMessage) const
{
    errorMessage.clear();
    QSqlDatabase db;
    if (!ensureDatabase(db, errorMessage)) return false;

    if (!db.transaction()) {
        errorMessage = QStringLiteral("无法开启事务：") + db.lastError().text();
        return false;
    }

    QSqlQuery q(db);
    q.prepare("UPDATE charger SET status = 2 WHERE id = :id");
    q.bindValue(":id", chargerId);
    if (!q.exec() || q.numRowsAffected() != 1) {
        errorMessage = QStringLiteral("标记故障失败：") + q.lastError().text();
        db.rollback();
        return false;
    }

    if (!writeOpsLog(db, QStringLiteral("标记电桩故障：#%1").arg(chargerId), errorMessage)) {
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        errorMessage = QStringLiteral("提交操作失败：") + db.lastError().text();
        db.rollback();
        return false;
    }
    return true;
}

bool ChargerService::recover(int chargerId, QString &errorMessage) const
{
    errorMessage.clear();
    QSqlDatabase db;
    if (!ensureDatabase(db, errorMessage)) return false;

    if (!db.transaction()) {
        errorMessage = QStringLiteral("无法开启事务：") + db.lastError().text();
        return false;
    }

    QSqlQuery q(db);
    q.prepare("UPDATE charger SET status = 0 WHERE id = :id AND status = 2");
    q.bindValue(":id", chargerId);
    if (!q.exec() || q.numRowsAffected() != 1) {
        errorMessage = QStringLiteral("恢复电桩失败：") + q.lastError().text();
        db.rollback();
        return false;
    }

    if (!writeOpsLog(db, QStringLiteral("恢复电桩正常：#%1").arg(chargerId), errorMessage)) {
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        errorMessage = QStringLiteral("提交操作失败：") + db.lastError().text();
        db.rollback();
        return false;
    }
    return true;
}

bool ChargerService::restartCharger(int chargerId, QString &errorMessage) const
{
    errorMessage.clear();
    QSqlDatabase db;
    if (!ensureDatabase(db, errorMessage)) return false;

    if (!db.transaction()) {
        errorMessage = QStringLiteral("无法开启事务：") + db.lastError().text();
        return false;
    }

    QSqlQuery q(db);
    q.prepare("UPDATE charger SET status = 0 WHERE id = :id");
    q.bindValue(":id", chargerId);

    if (!q.exec() || q.numRowsAffected() != 1) {
        errorMessage = QStringLiteral("远程重启失败：") + q.lastError().text();
        db.rollback();
        return false;
    }

    if (!writeOpsLog(db, QStringLiteral("远程重启电桩：#%1，状态置为空闲").arg(chargerId), errorMessage)) {
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        errorMessage = QStringLiteral("提交重启操作失败：") + db.lastError().text();
        db.rollback();
        return false;
    }
    return true;
}

bool ChargerService::addCharger(
    int stationId,
    const QString &chargerNo,
    int type,
    double power,
    QString &errorMessage) const
{
    errorMessage.clear();
    QSqlDatabase db;
    if (!ensureDatabase(db, errorMessage)) return false;

    if (chargerNo.trimmed().isEmpty()) {
        errorMessage = QStringLiteral("请输入电桩编号");
        return false;
    }
    if (power <= 0.0) {
        errorMessage = QStringLiteral("功率必须大于 0");
        return false;
    }

    if (!db.transaction()) {
        errorMessage = QStringLiteral("无法开启事务：") + db.lastError().text();
        return false;
    }

    QSqlQuery q(db);
    q.prepare(
        "INSERT INTO charger "
        "(station_id, charger_no, type, power, status, total_count, total_minutes) "
        "VALUES(:station_id, :charger_no, :type, :power, 0, 0, 0)");
    q.bindValue(":station_id", stationId);
    q.bindValue(":charger_no", chargerNo.trimmed());
    q.bindValue(":type", type);
    q.bindValue(":power", power);

    if (!q.exec()) {
        if (q.lastError().text().contains("UNIQUE", Qt::CaseInsensitive)) {
            errorMessage = QStringLiteral("电桩编号已存在");
        } else {
            errorMessage = QStringLiteral("新增电桩失败：") + q.lastError().text();
        }
        db.rollback();
        return false;
    }

    if (!writeOpsLog(db,
                     QStringLiteral("新增电桩：%1").arg(chargerNo.trimmed()),
                     errorMessage)) {
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        errorMessage = QStringLiteral("提交新增操作失败：") + db.lastError().text();
        db.rollback();
        return false;
    }
    return true;
}

bool ChargerService::deleteCharger(int chargerId, QString &errorMessage) const
{
    errorMessage.clear();
    QSqlDatabase db;
    if (!ensureDatabase(db, errorMessage)) return false;

    if (!db.transaction()) {
        errorMessage = QStringLiteral("无法开启事务：") + db.lastError().text();
        return false;
    }

    QSqlQuery check(db);
    check.prepare("SELECT charger_no, status FROM charger WHERE id = :id");
    check.bindValue(":id", chargerId);
    if (!check.exec() || !check.next()) {
        errorMessage = QStringLiteral("找不到指定电桩");
        db.rollback();
        return false;
    }

    const QString chargerNo = check.value(0).toString();
    const int status = check.value(1).toInt();

    if (status == 1) {
        errorMessage = QStringLiteral("该电桩正在使用中，禁止删除");
        db.rollback();
        return false;
    }

    QSqlQuery q(db);
    q.prepare("DELETE FROM charger WHERE id = :id AND status != 1");
    q.bindValue(":id", chargerId);
    if (!q.exec() || q.numRowsAffected() != 1) {
        errorMessage = QStringLiteral("删除电桩失败：") + q.lastError().text();
        db.rollback();
        return false;
    }

    if (!writeOpsLog(db,
                     QStringLiteral("删除电桩：%1").arg(chargerNo),
                     errorMessage)) {
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        errorMessage = QStringLiteral("提交删除操作失败：") + db.lastError().text();
        db.rollback();
        return false;
    }
    return true;
}
