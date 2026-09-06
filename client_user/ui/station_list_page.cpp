#include "station_list_page.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include "station_card_widget.h"

namespace client_user {

StationListPage::StationListPage(core::StationService *service, QWidget *parent)
    : QWidget(parent)
    , m_service(service)
{
    // ---------- 定位区 ----------
    m_regionCombo = new QComboBox(this);
    m_regionCombo->setObjectName(QStringLiteral("regionCombo"));
    m_regionCombo->setMinimumWidth(118);
    m_regionCombo->addItems(m_service->presetRegionNames());

    m_addressEdit = new QLineEdit(this);
    m_addressEdit->setObjectName(QStringLiteral("addressEdit"));
    m_addressEdit->setPlaceholderText(QStringLiteral("输入详细地址（可选）"));
    m_addressEdit->setClearButtonEnabled(true);

    m_locateBtn = new QPushButton(QStringLiteral("定位"), this);
    m_locateBtn->setObjectName(QStringLiteral("locateBtn"));
    m_locateBtn->setCursor(Qt::PointingHandCursor);

    auto *locateBar = new QHBoxLayout;
    locateBar->setSpacing(8);
    locateBar->addWidget(m_regionCombo);
    locateBar->addWidget(m_addressEdit, 1);
    locateBar->addWidget(m_locateBtn);

    // ---------- 位置信息 + 刷新 ----------
    m_locationLabel = new QLabel(QStringLiteral("尚未定位"), this);
    m_locationLabel->setObjectName(QStringLiteral("locationLabel"));
    m_locationLabel->setTextInteractionFlags(Qt::NoTextInteraction);

    m_refreshBtn = new QPushButton(QStringLiteral("刷新"), this);
    m_refreshBtn->setObjectName(QStringLiteral("refreshBtn"));
    m_refreshBtn->setCursor(Qt::PointingHandCursor);

    auto *infoBar = new QHBoxLayout;
    infoBar->addWidget(m_locationLabel, 1);
    infoBar->addWidget(m_refreshBtn);

    // ---------- 提示（toast） ----------
    m_toastLabel = new QLabel(this);
    m_toastLabel->setObjectName(QStringLiteral("toastLabel"));
    m_toastLabel->setWordWrap(true);
    m_toastLabel->setAlignment(Qt::AlignCenter);
    m_toastLabel->hide();

    m_toastTimer = new QTimer(this);
    m_toastTimer->setSingleShot(true);
    m_toastTimer->setInterval(4000);
    connect(m_toastTimer, &QTimer::timeout, m_toastLabel, &QLabel::hide);

    // ---------- 电站卡片滚动区 ----------
    m_cardsContainer = new QWidget(this);
    m_outerLayout = new QVBoxLayout(m_cardsContainer);
    m_outerLayout->setContentsMargins(10, 6, 10, 10);
    m_outerLayout->setSpacing(0);

    m_cardsLayout = new QVBoxLayout;
    m_cardsLayout->setSpacing(8);
    m_cardsLayout->addStretch(1);
    m_outerLayout->addLayout(m_cardsLayout, 1);

    // 空态占位文案（独立于卡片布局，避免被清空卡片时误删）
    m_emptyLabel = new QLabel(QStringLiteral("暂无充电站数据"), m_cardsContainer);
    m_emptyLabel->setObjectName(QStringLiteral("emptyHint"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->hide();
    m_outerLayout->addWidget(m_emptyLabel, 1);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName(QStringLiteral("cardScrollArea"));
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setWidget(m_cardsContainer);

    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(12, 12, 12, 8);
    pageLayout->setSpacing(6);
    pageLayout->addLayout(locateBar);
    pageLayout->addLayout(infoBar);
    pageLayout->addWidget(m_toastLabel);
    pageLayout->addWidget(m_scrollArea, 1);

    // ---------- 信号 ----------
    connect(m_locateBtn, &QPushButton::clicked, this,
            &StationListPage::onLocateClicked);
    connect(m_refreshBtn, &QPushButton::clicked, this, &StationListPage::refresh);
    connect(m_addressEdit, &QLineEdit::returnPressed, this,
            &StationListPage::onLocateClicked);
    connect(m_service, &core::StationService::located, this,
            &StationListPage::onLocated);
}

void StationListPage::setInitialRegion(const QString &regionName)
{
    const int index = m_regionCombo->findText(regionName);
    if (index >= 0) {
        m_regionCombo->setCurrentIndex(index);
    }
}

void StationListPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_firstShow) {
        m_firstShow = false;
        // 进入页面后自动按默认区域定位一次
        QTimer::singleShot(0, this, &StationListPage::onLocateClicked);
    } else {
        // 再次进入页面时刷新空闲数（联动管理端状态变化）
        refresh();
    }
}

void StationListPage::onLocateClicked()
{
    if (m_locating) {
        return;
    }
    const QString region = m_regionCombo->currentText();
    const QString address = m_addressEdit->text().trimmed();

    m_locating = true;
    m_locateBtn->setEnabled(false);
    m_locateBtn->setText(QStringLiteral("定位中…"));
    showToast(address.isEmpty() ? QStringLiteral("正在定位…")
                                : QStringLiteral("正在解析地址…"),
              false);
    m_service->locate(region, address);
}

void StationListPage::onLocated(const core::LocationResult &result)
{
    m_locating = false;
    m_locateBtn->setEnabled(true);
    m_locateBtn->setText(QStringLiteral("定位"));

    if (!result.ok) {
        showToast(result.message, true);
        return;
    }

    m_locationLabel->setText(QStringLiteral("当前位置：%1（%2, %3）")
                                 .arg(result.regionName)
                                 .arg(result.latitude, 0, 'f', 4)
                                 .arg(result.longitude, 0, 'f', 4));

    if (result.usedFallback) {
        // 地址解析失败或网络超时 → 已退化为预置坐标（UC-U-02 异常流 E1）
        showToast(result.message, true);
    } else if (result.message.startsWith(QStringLiteral("定位成功"))) {
        showToast(result.message, false);
    }

    refresh();
}

QVector<core::StationListItem> StationListPage::currentList() const
{
    if (!m_service) {
        return {};
    }
    return m_service->listStationsByDistance(m_service->lastLatitude(),
                                             m_service->lastLongitude());
}

void StationListPage::refresh()
{
    const QVector<core::StationListItem> items = currentList();
    rebuildList(items);
}

void StationListPage::rebuildList(const QVector<core::StationListItem> &items)
{
    clearCards();

    if (items.isEmpty()) {
        // 电站表为空 → 占位文案（UC-U-02 异常流 E2）
        m_emptyLabel->show();
        return;
    }
    m_emptyLabel->hide();

    for (const core::StationListItem &item : items) {
        auto *card = new StationCardWidget(item, m_cardsContainer);
        // 卡片点击 → 转发完整卡片信息（电站详情页为 UC-U-03 范畴，由主窗口处理）
        connect(card, &StationCardWidget::clicked, this,
                [this, item](int) { emit stationClicked(item); });
        m_cardsLayout->insertWidget(m_cardsLayout->count() - 1, card);
    }
}

void StationListPage::clearCards()
{
    while (QLayoutItem *child = m_cardsLayout->takeAt(0)) {
        if (QWidget *w = child->widget()) {
            w->deleteLater();
        }
        delete child;
    }
    m_cardsLayout->addStretch(1);
}

void StationListPage::showToast(const QString &text, bool isError)
{
    m_toastLabel->setText(text);
    m_toastLabel->setProperty("error", isError);
    // 刷新样式表属性
    m_toastLabel->style()->unpolish(m_toastLabel);
    m_toastLabel->style()->polish(m_toastLabel);
    m_toastLabel->show();
    m_toastTimer->start();
}

} // namespace client_user
