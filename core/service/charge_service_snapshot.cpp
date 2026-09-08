#include "charge_service.h"
#include "util/app_paths.h"

#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <cmath>

namespace core {

bool ChargeService::ensureChargingSnapshot(
    int userId,
    int orderId,
    QString &errorMessage)
{
    errorMessage.clear();

    if (userId <= 0 || orderId <= 0) {
        errorMessage = QStringLiteral("用户或订单信息无效");
        return false;
    }

    int timeScale = 60;
    const QString path =
        resolveDataFile(QStringLiteral("config/app.ini"));

    if (!path.isEmpty()) {
        QSettings settings(path, QSettings::IniFormat);

        bool ok = false;
        timeScale = settings.value(
            "charge/time_scale", 60
        ).toInt(&ok);

        if (!ok || timeScale <= 0
            || settings.status() != QSettings::NoError) {
            errorMessage = QStringLiteral("充电时间倍率配置无效");
            return false;
        }
    }

    QSqlDatabase db =
        QSqlDatabase::database("ncs_connection");

    if (!db.isValid() || !db.isOpen()) {
        errorMessage = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery begin(db);
    if (!begin.exec("BEGIN IMMEDIATE")) {
        errorMessage = QStringLiteral("无法开始事务：")
                       + begin.lastError().text();
        return false;
    }

    auto fail = [&](const QString &message) {
        db.rollback();
        errorMessage = message;
        return false;
    };

    QSqlQuery update(db);

    // 已保存的值保持不变，只补齐空值。
    if (!update.prepare(
            "UPDATE charging_order "
            "SET unit_price = COALESCE(unit_price, ("
            " SELECT s.price FROM charger c "
            " JOIN station s ON s.id = c.station_id "
            " WHERE c.id = charging_order.charger_id"
            ")), "
            "power_snapshot = COALESCE(power_snapshot, ("
            " SELECT c.power FROM charger c "
            " WHERE c.id = charging_order.charger_id"
            ")), "
            "time_scale_snapshot = "
            "COALESCE(time_scale_snapshot, :scale) "
            "WHERE id = :orderId AND user_id = :userId "
            "AND status = 1")) {
        return fail(
            QStringLiteral("准备计费参数保存失败：")
            + update.lastError().text());
    }

    update.bindValue(":scale", timeScale);
    update.bindValue(":orderId", orderId);
    update.bindValue(":userId", userId);

    if (!update.exec()) {
        return fail(
            QStringLiteral("保存计费参数失败：")
            + update.lastError().text());
    }

    QSqlQuery check(db);

    if (!check.prepare(
            "SELECT unit_price, power_snapshot, "
            "time_scale_snapshot FROM charging_order "
            "WHERE id = :orderId AND user_id = :userId "
            "AND status = 1")) {
        return fail(check.lastError().text());
    }

    check.bindValue(":orderId", orderId);
    check.bindValue(":userId", userId);

    if (!check.exec()) {
        return fail(check.lastError().text());
    }

    if (!check.next()) {
        return fail(
            QStringLiteral("订单不存在或已不在充电中，请刷新"));
    }

    bool priceOk = false;
    bool powerOk = false;
    bool scaleOk = false;

    const double price =
        check.value(0).toDouble(&priceOk);
    const double power =
        check.value(1).toDouble(&powerOk);
    const int scale =
        check.value(2).toInt(&scaleOk);

    if (!priceOk || !powerOk || !scaleOk
        || !std::isfinite(price)
        || !std::isfinite(power)
        || price < 0 || power <= 0 || scale <= 0) {
        return fail(
            QStringLiteral("电站单价、电桩功率或时间倍率异常"));
    }

    check.finish();

    if (!db.commit()) {
        const QString reason = db.lastError().text();
        return fail(
            QStringLiteral("保存计费参数失败：") + reason);
    }

    return true;
}

} // namespace core
