#include "geocoder.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

#include "../util/logger.h"

namespace core {

namespace {
const char *kModule = "Geocoder";
} // namespace

Geocoder::Geocoder(QObject *parent)
    : QObject(parent)
{
    m_timeoutTimer.setSingleShot(true);
    m_timeoutTimer.setInterval(kTimeoutMs);
    connect(&m_timeoutTimer, &QTimer::timeout, this, &Geocoder::onTimeout);
    connect(&m_manager, &QNetworkAccessManager::finished, this,
            &Geocoder::onReplyFinished);
}

Geocoder::~Geocoder()
{
    abortCurrentReply();
}

void Geocoder::geocode(const QString &address, const QString &key)
{
    abortCurrentReply();

    QUrl url(QStringLiteral("https://apis.map.qq.com/ws/geocoder/v1/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("address"), address);
    query.addQueryItem(QStringLiteral("key"), key);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setTransferTimeout(kTimeoutMs); // Qt 6.5 支持网络层超时
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));

    LOG_INFO(kModule, QStringLiteral("发起地理编码请求: address=%1").arg(address));
    m_reply = m_manager.get(request);
    m_timeoutTimer.start();
}

void Geocoder::onTimeout()
{
    LOG_WARN(kModule, QStringLiteral("地理编码请求超时(%1ms)").arg(kTimeoutMs));
    abortCurrentReply();
    finish(false, 0.0, 0.0, QStringLiteral("网络超时"));
}

void Geocoder::onReplyFinished(QNetworkReply *reply)
{
    if (!reply || reply != m_reply) {
        return;
    }
    m_timeoutTimer.stop();
    m_reply = nullptr;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        LOG_WARN(kModule,
                 QStringLiteral("地理编码网络错误: %1").arg(reply->errorString()));
        finish(false, 0.0, 0.0, reply->errorString());
        return;
    }

    const QByteArray body = reply->readAll();
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        LOG_WARN(kModule, QStringLiteral("地理编码响应解析失败"));
        finish(false, 0.0, 0.0, QStringLiteral("响应解析失败"));
        return;
    }

    const QJsonObject root = doc.object();
    const int status = root.value(QStringLiteral("status")).toInt(-1);
    if (status != 0) {
        const QString message = root.value(QStringLiteral("message")).toString();
        LOG_WARN(kModule,
                 QStringLiteral("地理编码接口返回错误: status=%1 message=%2")
                     .arg(status)
                     .arg(message));
        finish(false, 0.0, 0.0,
               QStringLiteral("地址解析失败(status=%1)").arg(status));
        return;
    }

    const QJsonObject location =
        root.value(QStringLiteral("result"))
            .toObject()
            .value(QStringLiteral("location"))
            .toObject();
    const double lat = location.value(QStringLiteral("lat")).toDouble();
    const double lng = location.value(QStringLiteral("lng")).toDouble();
    LOG_INFO(kModule,
             QStringLiteral("地理编码成功: (%1, %2)").arg(lat).arg(lng));
    finish(true, lat, lng, QString());
}

void Geocoder::abortCurrentReply()
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_timeoutTimer.stop();
}

void Geocoder::finish(bool ok, double lat, double lng, const QString &error)
{
    emit finished(ok, lat, lng, error);
}

} // namespace core
