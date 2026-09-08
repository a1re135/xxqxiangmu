#include "navigationdialog.h"
#include "util/geo_util.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QVariant>
#include <cmath>

#if NCS_HAS_WEBENGINE
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineSettings>

#include <QJsonDocument>
#include <QJsonObject>
#endif

namespace {

// 检查经纬度是否有效。
bool validCoordinate(double latitude, double longitude)
{
    return std::isfinite(latitude)
        && std::isfinite(longitude)
        && latitude >= -90.0
        && latitude <= 90.0
        && longitude >= -180.0
        && longitude <= 180.0;
}

} // namespace

NavigationDialog::NavigationDialog(
    const core::StationListItem &station,
    double startLatitude,
    double startLongitude,
    const QString &startName,
    QWidget *parent
)
    : QDialog(parent),
      m_station(station),
      m_startLatitude(startLatitude),
      m_startLongitude(startLongitude),
      m_startName(startName)
{
    setWindowTitle(QStringLiteral("路线导航"));
    setFixedSize(420, 760);

    if (m_startName.trimmed().isEmpty()) {
        m_startName = QStringLiteral("首页定位点");
    }

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 18, 16, 16);
    mainLayout->setSpacing(12);

    // ==============================
    // 1. 标题和出行方式
    // ==============================

    auto *title = new QLabel(
        QStringLiteral("路线导航"), this);
    title->setObjectName("pageTitle");
    mainLayout->addWidget(title);

    auto *modeLabel = new QLabel(
        QStringLiteral("选择出行方式"), this);
    mainLayout->addWidget(modeLabel);

    m_travelMode = new QComboBox(this);
    m_travelMode->addItem(QStringLiteral("驾车"), "drive");
    m_travelMode->addItem(QStringLiteral("步行"), "walk");
    m_travelMode->addItem(QStringLiteral("公交"), "bus");

    mainLayout->addWidget(m_travelMode);
    // 网络请求管理器，随导航窗口一起销毁。
    m_networkManager = new QNetworkAccessManager(this);

    // 单独显示网络状态，避免被其他操作提示覆盖。
    m_networkLabel = new QLabel(this);
    m_networkLabel->setTextFormat(Qt::PlainText);
    m_networkLabel->setWordWrap(true);
    mainLayout->addWidget(m_networkLabel);

    auto *retryNetworkButton = new QPushButton(
        QStringLiteral("重新检查网络"),
        this
    );

    retryNetworkButton->setMinimumHeight(32);
    retryNetworkButton->setCursor(Qt::PointingHandCursor);

    mainLayout->addWidget(retryNetworkButton);

    connect(
        retryNetworkButton,
        &QPushButton::clicked,
        this,
        &NavigationDialog::checkNetwork
    );

    // ==============================
    // 2. 可滚动的信息区域
    // ==============================

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff);

    auto *content = new QWidget;
    content->setObjectName("navigationContent");

    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 4, 0);
    contentLayout->setSpacing(12);

    // 创建一个信息卡片。
    auto addCard =
        [content, contentLayout](
            const QString &heading,
            const QString &description)
    {
        auto *card = new QFrame(content);
        card->setObjectName("infoCard");

        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(14, 14, 14, 14);
        cardLayout->setSpacing(8);

        auto *headingLabel = new QLabel(heading, card);
        headingLabel->setObjectName("cardHeading");
        headingLabel->setTextFormat(Qt::PlainText);
        headingLabel->setWordWrap(true);

        auto *descriptionLabel = new QLabel(
            description, card);
        descriptionLabel->setTextFormat(Qt::PlainText);
        descriptionLabel->setWordWrap(true);

        // 长文字允许换行，不把窗口撑宽。
        descriptionLabel->setMinimumWidth(0);

        cardLayout->addWidget(headingLabel);
        cardLayout->addWidget(descriptionLabel);
        contentLayout->addWidget(card);
    };

    addCard(
        QStringLiteral("起点 · 首页定位位置"),
        QStringLiteral("%1\n纬度：%2\n经度：%3")
            .arg(m_startName)
            .arg(m_startLatitude, 0, 'f', 6)
            .arg(m_startLongitude, 0, 'f', 6)
    );

    addCard(
        QStringLiteral("终点 · 目标电站"),
        QStringLiteral("%1\n%2\n纬度：%3\n经度：%4")
            .arg(m_station.name)
            .arg(m_station.address)
            .arg(m_station.latitude, 0, 'f', 6)
            .arg(m_station.longitude, 0, 'f', 6)
    );

    const bool coordinatesValid =
        validCoordinate(
            m_startLatitude, m_startLongitude)
        && validCoordinate(
            m_station.latitude, m_station.longitude);

    if (coordinatesValid) {
        const double distance =
            core::GeoUtil::haversineKm(
                m_startLatitude,
                m_startLongitude,
                m_station.latitude,
                m_station.longitude
            );

        addCard(
            QStringLiteral("直线距离"),
            QStringLiteral(
                "%1 公里\n"
                "按起终点坐标计算，实际出行距离以地图路线为准。")
                .arg(distance, 0, 'f', 1)
        );
    } else {
        addCard(
            QStringLiteral("坐标异常"),
            QStringLiteral(
                "无法计算距离，请返回首页重新定位，"
                "并检查电站坐标。")
        );
    }

    auto *hint = new QLabel(
        QStringLiteral(
            "当前显示导航信息卡。联网后可在系统浏览器中"
            "查看路线；离线时仍可查看以上坐标和直线距离。"),
        content
    );

    hint->setObjectName("hint");
    hint->setWordWrap(true);
    // 修改原提示，适用于内嵌地图和信息卡两种情况。
    hint->setText(
        QStringLiteral(
            "以下为起终点信息和直线距离。"
            "内嵌地图不可用时，可尝试在系统浏览器中查看路线。")
    );

    contentLayout->addWidget(hint);

    #if NCS_HAS_WEBENGINE

    const QString mapKey =
        core::StationService::loadTencentKey();

    if (!mapKey.isEmpty() && coordinatesValid) {
        // 地图放在滚动区域最上方，不改变420×760窗口尺寸。
        auto *mapPanel = new QWidget(content);
        auto *mapLayout = new QVBoxLayout(mapPanel);

        mapLayout->setContentsMargins(0, 0, 0, 0);
        mapLayout->setSpacing(6);

        auto *mapStatus = new QLabel(
            QStringLiteral("正在加载内嵌地图……"),
            mapPanel
        );

        mapStatus->setWordWrap(true);
        mapStatus->setStyleSheet(
            "color: #93C5FD; font-size: 12px;"
        );

        auto *mapView = new QWebEngineView(mapPanel);
        mapView->setMinimumWidth(0);
        mapView->setFixedHeight(260);

        // 本地HTML需要加载腾讯的远程脚本和地图资源。
        mapView->settings()->setAttribute(
            QWebEngineSettings::LocalContentCanAccessRemoteUrls,
            true
        );

        mapLayout->addWidget(mapStatus);
        mapLayout->addWidget(mapView);

        contentLayout->insertWidget(0, mapPanel);

        auto *mapTimeout = new QTimer(mapView);
        mapTimeout->setSingleShot(true);

        // 统一的失败处理：停止加载、隐藏地图，保留信息卡。
        auto showMapFallback =
            [mapView, mapStatus, mapTimeout]()
        {
            if (mapView->property("ncsFailed").toBool()) {
                return;
            }

            // 先标记，避免停止页面时重复进入失败处理。
            mapView->setProperty("ncsFailed", true);

            mapTimeout->stop();

            mapStatus->setStyleSheet(
                "color: #FB923C; font-size: 12px;"
            );

            mapStatus->setText(
                QStringLiteral(
                    "内嵌地图加载失败或超时，已保留坐标信息卡。"
                    "请检查网络、Key权限或浏览器图形支持。")
            );

            mapView->stop();
            mapView->hide();

            // 清除页面，结束后续脚本和地图加载。
            mapView->setUrl(QUrl(QStringLiteral("about:blank")));
        };

        connect(
            mapTimeout,
            &QTimer::timeout,
            mapView,
            showMapFallback
        );

        // HTML加载成功，不等于腾讯地图加载成功。
        // 这里只处理本地页面本身的加载失败。
        connect(
            mapView,
            &QWebEngineView::loadFinished,
            mapView,
            [showMapFallback](bool ok)
            {
                if (!ok) {
                    showMapFallback();
                }
            }
        );

        // HTML通过标题变化报告地图状态。
        connect(
            mapView,
            &QWebEngineView::titleChanged,
            mapView,
            [mapView, mapStatus, mapTimeout,
             showMapFallback](const QString &title)
            {
                if (mapView->property("ncsFailed").toBool()) {
                    return;
                }

                if (title == QStringLiteral("NCS_MAP_FAILED")) {
                    showMapFallback();
                    return;
                }

                if (title == QStringLiteral("NCS_MAP_READY")) {
                    mapTimeout->stop();

                    mapStatus->setStyleSheet(
                        "color: #93C5FD; font-size: 12px;"
                    );

                    mapStatus->setText(
                        QStringLiteral(
                            "底图已加载，准备查询路线……")
                    );
                    return;
                }

                if (!title.startsWith(
                        QStringLiteral("NCS_ROUTE_"))) {
                    return;
                }

                mapTimeout->stop();

                const int separator = title.indexOf('|');

                if (separator < 0) {
                    return;
                }

                const QString state = title.left(separator);

                const QString message =
                    QUrl::fromPercentEncoding(
                        title.mid(separator + 1).toUtf8()
                    );

                mapStatus->setTextFormat(Qt::PlainText);
                mapStatus->setText(message);

                if (state == QStringLiteral("NCS_ROUTE_OK")) {
                    mapStatus->setStyleSheet(
                        "color: #4ADE80; font-size: 12px;"
                    );
                } else if (
                    state == QStringLiteral("NCS_ROUTE_FAILED")) {

                    // 查询失败仍保留底图和信息卡。
                    mapStatus->setStyleSheet(
                        "color: #FB923C; font-size: 12px;"
                    );
                } else {
                    mapStatus->setStyleSheet(
                        "color: #93C5FD; font-size: 12px;"
                    );
                }
            }
        );

        // WebEngine渲染进程异常时，也保留信息卡。
        connect(
            mapView->page(),
            &QWebEnginePage::renderProcessTerminated,
            mapView,
            [showMapFallback]()
            {
                showMapFallback();
            }
        );

        // 使用JSON传递数据，不把电站名称直接拼进JS代码。
        QJsonObject mapConfig;
        mapConfig.insert("key", mapKey);
        mapConfig.insert("startLat", m_startLatitude);
        mapConfig.insert("startLng", m_startLongitude);
        mapConfig.insert(
            "mode",
            m_travelMode->currentData().toString()
        );
        mapConfig.insert("endLat", m_station.latitude);
        mapConfig.insert("endLng", m_station.longitude);
        mapConfig.insert(
            "mode",
            m_travelMode->currentData().toString()
        );
        const QString configText = QString::fromUtf8(
            QJsonDocument(mapConfig).toJson(
                QJsonDocument::Compact)
        );

        QUrl mapUrl(
            QStringLiteral("qrc:/maps/navigation.html")
        );

        mapUrl.setFragment(configText);
        // 将当前方式传给HTML。
        auto querySelectedRoute = [this, mapView]()
        {
            if (mapView->property("ncsFailed").toBool()) {
                return;
            }

            // 只允许预设值，避免任意内容进入JS代码。
            const QString mode =
                m_travelMode->currentData().toString();

            if (mode != "drive"
                && mode != "walk"
                && mode != "bus") {
                return;
            }

            const QString script = QStringLiteral(
                "window.ncsPendingMode = '%1';"
                "if (typeof window.ncsSetMode === 'function') {"
                "    window.ncsSetMode('%1');"
                "}"
            ).arg(mode);

            mapView->page()->runJavaScript(script);
        };

        // 快速切换时稍等一下，减少重复调用。
        auto *modeTimer = new QTimer(mapView);
        modeTimer->setSingleShot(true);
        modeTimer->setInterval(350);

        connect(
            modeTimer,
            &QTimer::timeout,
            mapView,
            querySelectedRoute
        );

        connect(
            m_travelMode,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            mapView,
            [modeTimer](int)
            {
                modeTimer->start();
            }
        );

        // 页面刚加载好时，同步最新选择。
        // 地图尚未就绪时，HTML会先记录方式。
        connect(
            mapView,
            &QWebEngineView::loadFinished,
            mapView,
            [querySelectedRoute](bool ok)
            {
                if (ok) {
                    querySelectedRoute();
                }
            }
        );

        auto *retryRouteButton = new QPushButton(
            QStringLiteral("重新查询路线"),
            mapPanel
        );

        retryRouteButton->setMinimumHeight(32);
        retryRouteButton->setCursor(Qt::PointingHandCursor);
        mapLayout->addWidget(retryRouteButton);

        connect(
            retryRouteButton,
            &QPushButton::clicked,
            mapView,
            [modeTimer]()
            {
                modeTimer->start();
            }
        );

        mapTimeout->start(15000);
        mapView->load(mapUrl);

    } else {
        auto *fallbackLabel = new QLabel(
            QStringLiteral(
                "未配置地图Key或坐标无效，当前使用坐标信息卡。"),
            content
        );

        fallbackLabel->setWordWrap(true);
        contentLayout->insertWidget(0, fallbackLabel);
    }

    #else

    auto *fallbackLabel = new QLabel(
        QStringLiteral(
            "当前版本未安装WebEngine，使用坐标信息卡和浏览器导航。"),
        content
    );

    fallbackLabel->setWordWrap(true);
    contentLayout->insertWidget(0, fallbackLabel);

    #endif

    contentLayout->addStretch();

    scrollArea->setWidget(content);
    mainLayout->addWidget(scrollArea, 1);

    // ==============================
    // 3. 提示和底部按钮
    // ==============================

    m_messageLabel = new QLabel(this);
    m_messageLabel->setObjectName("hint");
    m_messageLabel->setTextFormat(Qt::PlainText);
    m_messageLabel->setWordWrap(true);
    mainLayout->addWidget(m_messageLabel);

    auto *browserButton = new QPushButton(
        QStringLiteral("在系统浏览器中打开"), this);
    browserButton->setObjectName("primaryButton");
    browserButton->setMinimumHeight(44);
    browserButton->setCursor(Qt::PointingHandCursor);
    browserButton->setEnabled(coordinatesValid);

    auto *backButton = new QPushButton(
        QStringLiteral("返回"), this);
    backButton->setMinimumHeight(40);
    backButton->setCursor(Qt::PointingHandCursor);

    mainLayout->addWidget(browserButton);
    mainLayout->addWidget(backButton);

    // ==============================
    // 4. 深蓝主题
    // ==============================

    setStyleSheet(QStringLiteral(R"(
        QDialog, QWidget#navigationContent {
            background-color: #0B1220;
        }

        QLabel {
            color: #CBD5E1;
            font-size: 13px;
            background: transparent;
        }

        QLabel#pageTitle {
            color: #F2F5FA;
            font-size: 24px;
            font-weight: bold;
        }

        QLabel#cardHeading {
            color: #93C5FD;
            font-size: 15px;
            font-weight: bold;
        }

        QLabel#hint {
            color: #94A3B8;
            font-size: 12px;
        }

        QFrame#infoCard {
            background-color: #111E33;
            border: 1px solid #293C55;
            border-radius: 12px;
        }

        QComboBox {
            background-color: #14243C;
            color: #F2F5FA;
            border: 1px solid #345174;
            border-radius: 10px;
            padding: 10px;
            font-size: 14px;
        }

        QComboBox QAbstractItemView {
            background-color: #14243C;
            color: white;
            selection-background-color: #2563EB;
            selection-color: white;
        }

        QScrollArea {
            background: transparent;
            border: none;
        }

        QPushButton {
            background-color: #14243C;
            color: #BFDBFE;
            border: 1px solid #345174;
            border-radius: 10px;
            font-size: 14px;
        }

        QPushButton:hover {
            background-color: #24456E;
        }

        QPushButton#primaryButton {
            background-color: #2563EB;
            color: white;
            border: 1px solid #3B82F6;
            font-weight: bold;
        }

        QPushButton#primaryButton:hover {
            background-color: #3B82F6;
        }

        QPushButton:disabled {
            background-color: #1C2C45;
            color: #788CA6;
            border-color: #263853;
        }
    )"));

    connect(
        browserButton,
        &QPushButton::clicked,
        this,
        &NavigationDialog::openBrowserNavigation
    );

    connect(
        backButton,
        &QPushButton::clicked,
        this,
        &QDialog::reject
    );

    // 腾讯网页URI的步行模式主要面向移动端。
    auto updateModeHint = [this]()
    {
        if (m_travelMode->currentData().toString() == "walk") {
            m_messageLabel->setText(
                QStringLiteral(
                    "步行模式面向移动端；"
                    "电脑浏览器可能不支持此模式。")
            );
        } else {
            m_messageLabel->clear();
        }
    };

    connect(
        m_travelMode,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this,
        [updateModeHint](int) {
            updateModeHint();
        }
    );
    // 等页面初始化完后自动检查一次。
    QTimer::singleShot(
        0,
        this,
        &NavigationDialog::checkNetwork
    );
}

void NavigationDialog::openBrowserNavigation()
{
    if (!validCoordinate(
            m_startLatitude, m_startLongitude)
        || !validCoordinate(
            m_station.latitude, m_station.longitude)) {

        m_messageLabel->setText(
            QStringLiteral("坐标无效，无法打开路线。"));
        return;
    }

    QUrl url(QStringLiteral(
        "https://apis.map.qq.com/uri/v1/routeplan"
    ));

    QUrlQuery query;

    query.addQueryItem(
        QStringLiteral("type"),
        m_travelMode->currentData().toString()
    );

    query.addQueryItem(
        QStringLiteral("from"),
        m_startName
    );

    query.addQueryItem(
        QStringLiteral("fromcoord"),
        QStringLiteral("%1,%2")
            .arg(m_startLatitude, 0, 'f', 6)
            .arg(m_startLongitude, 0, 'f', 6)
    );

    query.addQueryItem(
        QStringLiteral("to"),
        m_station.name
    );

    query.addQueryItem(
        QStringLiteral("tocoord"),
        QStringLiteral("%1,%2")
            .arg(m_station.latitude, 0, 'f', 6)
            .arg(m_station.longitude, 0, 'f', 6)
    );

    query.addQueryItem(
        QStringLiteral("coord_type"),
        QStringLiteral("2")
    );

    query.addQueryItem(
        QStringLiteral("policy"),
        QStringLiteral("0")
    );

    query.addQueryItem(
        QStringLiteral("referer"),
        QStringLiteral("NCS_Charging_Platform")
    );

    url.setQuery(query);

    if (!QDesktopServices::openUrl(url)) {
        m_messageLabel->setText(
            QStringLiteral(
                "无法打开浏览器，请检查系统默认浏览器设置。")
        );
        return;
    }

    // openUrl成功只代表系统接受了打开请求，
    // 不代表网页已经联网加载成功。
    m_messageLabel->setText(
        QStringLiteral(
            "已请求浏览器打开路线。"
            "若网页无法加载，请检查网络连接。")
    );
}

void NavigationDialog::checkNetwork()
{
    // 防止连续点击发起重复检查。
    if (m_networkChecking) {
        return;
    }

    m_networkChecking = true;

    m_networkLabel->setStyleSheet(
        "color: #93C5FD; font-size: 12px;"
    );
    m_networkLabel->setText(
        QStringLiteral("正在检查腾讯地图连接……")
    );

    // 仅检查地图网站连接，不调用地址解析接口。
    QNetworkRequest request{
        QUrl(QStringLiteral("https://map.qq.com/"))
    };

    request.setAttribute(
        QNetworkRequest::CacheLoadControlAttribute,
        QNetworkRequest::AlwaysNetwork
    );

    // HEAD只请求响应头，不下载完整地图页面。
    QNetworkReply *reply =
        m_networkManager->head(request);

    // 整次检查最多等待5秒。
    auto *timeoutTimer = new QTimer(reply);
    timeoutTimer->setSingleShot(true);

    connect(
        timeoutTimer,
        &QTimer::timeout,
        reply,
        [reply]()
        {
            if (reply->isFinished()) {
                return;
            }

            // 必须先记录超时，再取消请求。
            // abort会触发finished，后续统一在那里处理。
            reply->setProperty("probeTimedOut", true);
            reply->abort();
        }
    );

    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply, timeoutTimer]()
        {
            timeoutTimer->stop();
            m_networkChecking = false;

            const bool timedOut =
                reply->property("probeTimedOut").toBool();

            const int httpStatus =
                reply->attribute(
                    QNetworkRequest::HttpStatusCodeAttribute
                ).toInt();

            if (timedOut) {
                m_networkLabel->setStyleSheet(
                    "color: #FB923C; font-size: 12px;"
                );

                m_networkLabel->setText(
                    QStringLiteral(
                        "地图连接超时，暂时无法确认在线路线服务。"
                        "坐标信息卡仍可使用，请检查网络后重试。")
                );
            } else if (
                reply->error() == QNetworkReply::NoError
                && httpStatus >= 200
                && httpStatus < 300
            ) {
                m_networkLabel->setStyleSheet(
                    "color: #4ADE80; font-size: 12px;"
                );

                m_networkLabel->setText(
                    QStringLiteral(
                        "腾讯地图网站连接正常，"
                        "可尝试在浏览器中查看路线。")
                );
            } else if (httpStatus > 0) {
                // 收到了服务器响应，不能直接判断为断网。
                m_networkLabel->setStyleSheet(
                    "color: #FB923C; font-size: 12px;"
                );

                m_networkLabel->setText(
                    QStringLiteral(
                        "地图网站返回 HTTP %1，"
                        "暂时无法确认服务可用。"
                        "你仍可尝试在浏览器中打开。")
                        .arg(httpStatus)
                );
            } else {
                m_networkLabel->setStyleSheet(
                    "color: #F87171; font-size: 12px;"
                );

                m_networkLabel->setText(
                    QStringLiteral(
                        "无法连接腾讯地图，请检查网络、DNS或代理设置。"
                        "坐标信息卡仍可使用。")
                );
            }

            // 在完成信号处理结束后释放请求。
            reply->deleteLater();
        }
    );

    timeoutTimer->start(5000);
}
