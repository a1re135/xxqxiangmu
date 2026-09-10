#include "userservice.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QDateTime>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace {
bool getOpenDb(QSqlDatabase &db, QString &errorMessage)
{
    db = QSqlDatabase::database("ncs_connection");
    if (!db.isValid() || !db.isOpen()) {
        errorMessage = QStringLiteral("数据库未打开");
        return false;
    }
    return true;
}

void fillUser(QSqlQuery &query, UserInfo &userInfo)
{
    userInfo.id = query.value("id").toInt();
    userInfo.phone = query.value("phone").toString();
    userInfo.nickname = query.value("nickname").toString();
    userInfo.avatarPath = query.value("avatar_path").toString();
    userInfo.balance = query.value("balance").toDouble();
    userInfo.registerTime = query.value("register_time").toString();
    userInfo.status = query.value("status").toInt();
}
}

UserService::UserService() = default;

bool UserService::findUserByPhone(const QString &phone, UserInfo &userInfo, QString &errorMessage)
{
    errorMessage.clear();
    QSqlDatabase db;
    if (!getOpenDb(db, errorMessage)) return false;

    QSqlQuery query(db);
    query.prepare("SELECT id, phone, nickname, avatar_path, balance, register_time, status "
                  "FROM user WHERE phone=:phone LIMIT 1");
    query.bindValue(":phone", phone);

    if (!query.exec()) {
        errorMessage = QStringLiteral("查询用户失败：") + query.lastError().text();
        return false;
    }
    if (!query.next()) return false;

    fillUser(query, userInfo);
    return true;
}

bool UserService::createUser(const QString &phone, UserInfo &userInfo, QString &errorMessage)
{
    errorMessage.clear();
    QSqlDatabase db;
    if (!getOpenDb(db, errorMessage)) return false;

    const QString nickname = QStringLiteral("用户") + phone.right(4);
    const QString registerTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    if (!db.transaction()) {
        errorMessage = QStringLiteral("无法开始数据库事务：") + db.lastError().text();
        return false;
    }

    QSqlQuery query(db);
    query.prepare("INSERT INTO user(phone,nickname,avatar_path,balance,register_time,status) "
                  "VALUES(:phone,:nickname,NULL,0,:register_time,1)");
    query.bindValue(":phone", phone);
    query.bindValue(":nickname", nickname);
    query.bindValue(":register_time", registerTime);

    if (!query.exec()) {
        db.rollback();
        errorMessage = QStringLiteral("创建用户失败：") + query.lastError().text();
        return false;
    }

    const int id = query.lastInsertId().toInt();
    if (!db.commit()) {
        db.rollback();
        errorMessage = QStringLiteral("提交用户创建失败：") + db.lastError().text();
        return false;
    }

    userInfo.id = id;
    userInfo.phone = phone;
    userInfo.nickname = nickname;
    userInfo.avatarPath.clear();
    userInfo.balance = 0.0;
    userInfo.registerTime = registerTime;
    userInfo.status = 1;
    return true;
}

bool UserService::loginOrRegister(const QString &phone,
                                  const QString &verificationCode,
                                  const QString &expectedCode,
                                  UserInfo &userInfo,
                                  QString &errorMessage)
{
    errorMessage.clear();
    if (verificationCode != expectedCode) {
        errorMessage = QStringLiteral("验证码错误");
        return false;
    }

    UserInfo existingUser;
    if (!findUserByPhone(phone, existingUser, errorMessage)) {
        if (!errorMessage.isEmpty()) return false;
        return createUser(phone, userInfo, errorMessage);
    }

    if (existingUser.status == 0) {
        errorMessage = QStringLiteral("账号已被冻结，请联系客服");
        return false;
    }

    userInfo = existingUser;
    return true;
}

bool UserService::getUserById(int id, UserInfo &userInfo, QString &errorMessage)
{
    errorMessage.clear();
    QSqlDatabase db;
    if (!getOpenDb(db, errorMessage)) return false;

    QSqlQuery query(db);
    query.prepare("SELECT id, phone, nickname, avatar_path, balance, register_time, status "
                  "FROM user WHERE id=:id LIMIT 1");
    query.bindValue(":id", id);

    if (!query.exec()) {
        errorMessage = QStringLiteral("查询用户失败：") + query.lastError().text();
        return false;
    }
    if (!query.next()) {
        errorMessage = QStringLiteral("用户不存在");
        return false;
    }

    fillUser(query, userInfo);
    return true;
}

bool UserService::updateAvatar(
    int id,
    const QString &avatarPath,
    QString &errorMessage)
{
    errorMessage.clear();


    // ==========================================
    // Validate source file
    // ==========================================

    QFileInfo sourceInfo(
        avatarPath
    );

    if (!sourceInfo.exists()
        || !sourceInfo.isFile()) {

        errorMessage =
            QStringLiteral(
                "选择的头像文件不存在"
            );

        return false;
    }


    const QString extension =
        sourceInfo
            .suffix()
            .toLower();


    const QStringList allowedExtensions = {
        QStringLiteral("png"),
        QStringLiteral("jpg"),
        QStringLiteral("jpeg"),
        QStringLiteral("bmp")
    };


    if (!allowedExtensions.contains(
            extension)) {

        errorMessage =
            QStringLiteral(
                "头像仅支持 PNG、JPG、"
                "JPEG、BMP 格式"
            );

        return false;
    }


    constexpr qint64 maxAvatarSize =
        5LL * 1024LL * 1024LL;


    if (sourceInfo.size()
        > maxAvatarSize) {

        errorMessage =
            QStringLiteral(
                "图片过大，请选择 "
                "5MB 以内的图片"
            );

        return false;
    }


    // ==========================================
    // Application data directory
    // ==========================================

    const QString genericDataPath =
        QStandardPaths::writableLocation(
            QStandardPaths::
                GenericDataLocation
        );


    if (genericDataPath.isEmpty()) {
        errorMessage =
            QStringLiteral(
                "无法获取应用数据目录"
            );

        return false;
    }


    QDir genericDataDir(
        genericDataPath
    );


    const QString applicationDir =
        genericDataDir.filePath(
            QStringLiteral(
                "NCS_Charging_Platform"
            )
        );


    QDir appDir(
        applicationDir
    );


    if (!appDir.mkpath(
            QStringLiteral(
                "avatars"
            ))) {

        errorMessage =
            QStringLiteral(
                "无法创建头像目录"
            );

        return false;
    }


    // Save a RELATIVE path in SQLite.
    const QString relativePath =
        QStringLiteral(
            "avatars/user_%1.%2"
        )
            .arg(id)
            .arg(extension);


    const QString destinationPath =
        appDir.filePath(
            relativePath
        );


    // ==========================================
    // Copy avatar
    // ==========================================

    const QString sourceAbsolute =
        QDir::cleanPath(
            sourceInfo
                .absoluteFilePath()
        );

    const QString destinationAbsolute =
        QDir::cleanPath(
            QFileInfo(
                destinationPath
            ).absoluteFilePath()
        );


    if (sourceAbsolute
        != destinationAbsolute) {

        // Replace an existing avatar
        // with the same filename.
        if (QFile::exists(
                destinationPath)) {

            if (!QFile::remove(
                    destinationPath)) {

                errorMessage =
                    QStringLiteral(
                        "无法替换旧头像"
                    );

                return false;
            }
        }


        if (!QFile::copy(
                avatarPath,
                destinationPath)) {

            errorMessage =
                QStringLiteral(
                    "复制头像文件失败"
                );

            return false;
        }
    }


    // ==========================================
    // Save relative path to SQLite
    // ==========================================

    QSqlDatabase db;

    if (!getOpenDb(
            db,
            errorMessage)) {

        return false;
    }


    QSqlQuery query(db);

    query.prepare(
        "UPDATE user "
        "SET avatar_path = :avatar "
        "WHERE id = :id"
    );

    query.bindValue(
        ":avatar",
        relativePath
    );

    query.bindValue(
        ":id",
        id
    );


    if (!query.exec()) {
        errorMessage =
            QStringLiteral(
                "修改头像失败："
            )
            + query.lastError().text();

        return false;
    }


    if (query.numRowsAffected() != 1) {
        errorMessage =
            QStringLiteral(
                "修改头像失败："
                "未找到对应用户"
            );

        return false;
    }


    return true;
}

bool UserService::updateNickname(int id, const QString &nickname, QString &errorMessage)
{
    errorMessage.clear();
    const QString cleanNickname = nickname.trimmed();
    if (cleanNickname.isEmpty()) {
        errorMessage = QStringLiteral("昵称不能为空");
        return false;
    }
    if (cleanNickname.size() > 20) {
        errorMessage =
            QStringLiteral(
                "昵称长度不能超过20个字符"
            );

        return false;
    }

    QSqlDatabase db;
    if (!getOpenDb(db, errorMessage)) return false;

    // FIX: 昵称必须真正 UPDATE 到数据库；检查受影响行数，避免假成功。
    QSqlQuery query(db);
    query.prepare("UPDATE user SET nickname=:nickname WHERE id=:id");
    query.bindValue(":nickname", cleanNickname);
    query.bindValue(":id", id);

    if (!query.exec()) {
        errorMessage = QStringLiteral("修改昵称失败：") + query.lastError().text();
        return false;
    }
    if (query.numRowsAffected() != 1) {
        errorMessage = QStringLiteral("修改昵称失败：未找到对应用户");
        return false;
    }
    return true;
}

bool UserService::recharge(int id, double amount, double &newBalance, QString &errorMessage)
{
    errorMessage.clear();
    newBalance = 0.0;
    if (amount < 0.01
        || amount > 10000.00) {

        errorMessage =
            QStringLiteral(
                "充值金额必须在 "
                "0.01 - 10000.00 元之间"
            );

        return false;
    }

    QSqlDatabase db;
    if (!getOpenDb(db, errorMessage)) return false;

    // FIX: UPDATE + SELECT 放在同一事务中，确保余额更新和页面读到的是同一结果。
    if (!db.transaction()) {
        errorMessage = QStringLiteral("无法开始充值事务：") + db.lastError().text();
        return false;
    }

    QSqlQuery update(db);
    update.prepare("UPDATE user SET balance=ROUND(balance+:money, 2) WHERE id=:id");
    update.bindValue(":money", amount);
    update.bindValue(":id", id);

    if (!update.exec() || update.numRowsAffected() != 1) {
        const QString detail = update.lastError().text();
        db.rollback();
        errorMessage = detail.isEmpty()
            ? QStringLiteral("充值失败：未找到对应用户")
            : QStringLiteral("充值失败：") + detail;
        return false;
    }

    QSqlQuery select(db);
    select.prepare("SELECT balance FROM user WHERE id=:id");
    select.bindValue(":id", id);
    if (!select.exec() || !select.next()) {
        const QString detail = select.lastError().text();
        db.rollback();
        errorMessage = QStringLiteral("读取充值后余额失败：") + detail;
        return false;
    }

    newBalance = select.value(0).toDouble();

    if (!db.commit()) {
        db.rollback();
        errorMessage = QStringLiteral("提交充值失败：") + db.lastError().text();
        return false;
    }
    return true;
}

bool UserService::findUnfinishedOrder(
    int userId,
    int &orderId,
    QString &errorMessage)
{
    orderId = -1;
    errorMessage.clear();

    if (userId <= 0) {
        errorMessage = QStringLiteral("用户未登录");
        return false;
    }

    QSqlDatabase db;
    if (!getOpenDb(db, errorMessage)) {
        return false;
    }

    QSqlQuery query(db);

    if (!query.prepare(
            "SELECT id "
            "FROM charging_order "
            "WHERE user_id = :userId "
            "AND status IN (0, 1) "
            "ORDER BY id ASC "
            "LIMIT 1")) {
        errorMessage = QStringLiteral("准备订单查询失败：")
                       + query.lastError().text();
        return false;
    }

    query.bindValue(":userId", userId);

    if (!query.exec()) {
        errorMessage = QStringLiteral("查询未完成订单失败：")
                       + query.lastError().text();
        return false;
    }

    if (query.next()) {
        orderId = query.value(0).toInt();
    } else if (query.lastError().isValid()) {
        errorMessage = QStringLiteral("读取订单失败：")
                       + query.lastError().text();
        return false;
    }

    return true;
}

bool UserService::cancelReservation(
    int userId,
    int orderId,
    QString &errorMessage)
{
    errorMessage.clear();

    if (userId <= 0 || orderId <= 0) {
        errorMessage = QStringLiteral("用户或订单信息无效");
        return false;
    }

    QSqlDatabase db;
    if (!getOpenDb(db, errorMessage)) {
        return false;
    }

    // 订单修改与电桩释放必须一起成功。
    if (!db.transaction()) {
        errorMessage = QStringLiteral("无法开始事务：")
                       + db.lastError().text();
        return false;
    }

    auto fail = [&](const QString &message) {
        db.rollback();
        errorMessage = message;
        return false;
    };

    QSqlQuery updateOrder(db);

    if (!updateOrder.prepare(
            "UPDATE charging_order "
            "SET status = 3, end_time = :endTime "
            "WHERE id = :orderId "
            "AND user_id = :userId "
            "AND status = 0 "
            "AND COALESCE(start_time, '') = '' "
            "AND energy = 0 AND amount = 0")) {
        return fail(
            QStringLiteral("准备取消订单失败：")
            + updateOrder.lastError().text());
    }

    updateOrder.bindValue(
        ":endTime",
        QDateTime::currentDateTime().toString(
            "yyyy-MM-dd HH:mm:ss"));
    updateOrder.bindValue(":orderId", orderId);
    updateOrder.bindValue(":userId", userId);

    if (!updateOrder.exec()) {
        return fail(
            QStringLiteral("取消订单失败：")
            + updateOrder.lastError().text());
    }

    if (updateOrder.numRowsAffected() != 1) {
        return fail(
            QStringLiteral(
                "订单不存在、不属于当前用户，"
                "或已开始充电，不能取消预约。"));
    }

    QSqlQuery releaseCharger(db);

    // 只释放使用中的桩：
    // 不覆盖故障状态，也不释放仍有其他有效订单的桩。
    if (!releaseCharger.prepare(
            "UPDATE charger "
            "SET status = 0 "
            "WHERE id = ("
            "    SELECT charger_id FROM charging_order "
            "    WHERE id = :orderId AND user_id = :userId"
            ") "
            "AND status = 1 "
            "AND NOT EXISTS ("
            "    SELECT 1 FROM charging_order o "
            "    WHERE o.charger_id = charger.id "
            "    AND o.status IN (0, 1)"
            ")")) {
        return fail(
            QStringLiteral("准备释放电桩失败：")
            + releaseCharger.lastError().text());
    }

    releaseCharger.bindValue(":orderId", orderId);
    releaseCharger.bindValue(":userId", userId);

    if (!releaseCharger.exec()) {
        return fail(
            QStringLiteral("释放电桩失败：")
            + releaseCharger.lastError().text());
    }

    if (!db.commit()) {
        const QString reason = db.lastError().text();
        db.rollback();
        errorMessage = QStringLiteral("保存取消结果失败：")
                       + reason;
        return false;
    }

    return true;
}

bool UserService::startCharging(
    int userId,
    int stationId,
    int chargerId,
    int &orderId,
    QString &errorMessage)
{
    orderId = -1;
    errorMessage.clear();

    if (userId <= 0 || stationId <= 0 || chargerId <= 0) {
        errorMessage = QStringLiteral("用户或电桩信息无效");
        return false;
    }

    QSqlDatabase db;
    if (!getOpenDb(db, errorMessage)) {
        return false;
    }

    if (!db.transaction()) {
        errorMessage = QStringLiteral("无法开始事务：")
                       + db.lastError().text();
        return false;
    }

    auto fail = [&](const QString &message) {
        db.rollback();
        errorMessage = message;
        return false;
    };

    // 直接通过条件更新抢占空闲桩。
    // 避免查询时空闲、真正开始时已被其他用户占用。
    QSqlQuery occupy(db);

    if (!occupy.prepare(
            "UPDATE charger "
            "SET status = 1 "
            "WHERE id = :chargerId "
            "AND station_id = :stationId "
            "AND status = 0")) {
        return fail(
            QStringLiteral("准备更新电桩失败：")
            + occupy.lastError().text());
    }

    occupy.bindValue(":chargerId", chargerId);
    occupy.bindValue(":stationId", stationId);

    if (!occupy.exec()) {
        return fail(
            QStringLiteral("更新电桩失败：")
            + occupy.lastError().text());
    }

    if (occupy.numRowsAffected() != 1) {
        return fail(
            QStringLiteral(
                "电桩已被占用、发生故障或不属于本站，"
                "请刷新后重新选择。"));
    }

    // 在同一事务中再次检查未完成订单。
    // 即使绕过界面检查，也不能重复创建。
    QSqlQuery createOrder(db);

    if (!createOrder.prepare(
            "INSERT INTO charging_order "
            "(user_id, charger_id, start_time, "
            "energy, amount, status) "
            "SELECT :userId, :chargerId, :startTime, 0, 0, 1 "
            "WHERE EXISTS ("
            "    SELECT 1 FROM user "
            "    WHERE id = :userId AND status = 1"
            ") "
            "AND NOT EXISTS ("
            "    SELECT 1 FROM charging_order "
            "    WHERE user_id = :userId "
            "    AND status IN (0, 1)"
            ")")) {
        return fail(
            QStringLiteral("准备创建订单失败：")
            + createOrder.lastError().text());
    }

    createOrder.bindValue(":userId", userId);
    createOrder.bindValue(":chargerId", chargerId);
    createOrder.bindValue(
        ":startTime",
        QDateTime::currentDateTime().toString(
            "yyyy-MM-dd HH:mm:ss")
    );

    if (!createOrder.exec()) {
        return fail(
            QStringLiteral("创建订单失败：")
            + createOrder.lastError().text());
    }

    if (createOrder.numRowsAffected() != 1) {
        return fail(
            QStringLiteral(
                "用户不存在、账号不可用，"
                "或已有未完成订单，请先处理原订单。"));
    }

    bool validId = false;
    const int newOrderId =
        createOrder.lastInsertId().toInt(&validId);

    if (!validId || newOrderId <= 0) {
        return fail(QStringLiteral("无法获取新订单编号"));
    }

    if (!db.commit()) {
        const QString reason = db.lastError().text();
        db.rollback();

        errorMessage = QStringLiteral("保存充电订单失败：")
                       + reason;
        return false;
    }

    orderId = newOrderId;
    return true;
}
