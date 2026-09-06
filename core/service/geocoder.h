#pragma once
// ============================================================
// 腾讯地图地理编码客户端（UC-U-02）
// 调用腾讯位置服务 WebService 地理编码接口：
//   GET https://apis.map.qq.com/ws/geocoder/v1/?address=<地址>&key=<Key>
// 异步接口：发起后通过 finished 信号回调；内置 5 秒超时。
// ============================================================
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QTimer>

class QNetworkReply;

namespace core {

class Geocoder : public QObject {
    Q_OBJECT
public:
    explicit Geocoder(QObject *parent = nullptr);
    ~Geocoder() override;

    // 发起地理编码请求。同一时刻只允许一个请求。
    void geocode(const QString &address, const QString &key);

signals:
    // ok=true 时 latitude/longitude 有效；否则 errorMessage 说明原因
    void finished(bool ok, double latitude, double longitude, const QString &errorMessage);

private slots:
    void onReplyFinished(QNetworkReply *reply);
    void onTimeout();

private:
    void finish(bool ok, double lat, double lng, const QString &error);
    void abortCurrentReply();

    QNetworkAccessManager m_manager;
    QNetworkReply *m_reply = nullptr;
    QTimer m_timeoutTimer;
    static constexpr int kTimeoutMs = 5000;
};

} // namespace core
