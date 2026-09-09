#include "station_card_widget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>

namespace client_user {

StationCardWidget::StationCardWidget(const core::StationListItem &item,
                                     QWidget *parent)
    : QFrame(parent)
    , m_item(item)
{
    setupUi();
}

void StationCardWidget::setupUi()
{
    setObjectName(QStringLiteral("stationCard"));
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    auto *nameLabel = new QLabel(m_item.name, this);
    nameLabel->setObjectName(QStringLiteral("stationName"));
    nameLabel->setWordWrap(true);
    nameLabel->setTextFormat(Qt::PlainText);

    // 使用按钮显示距离，让它单独接收点击。
    auto *distLabel = new QPushButton(
        QStringLiteral("%1 km  ›")
            .arg(m_item.distanceKm, 0, 'f', 1),
        this
    );

    distLabel->setObjectName(QStringLiteral("distanceButton"));
    distLabel->setCursor(Qt::PointingHandCursor);
    distLabel->setToolTip(QStringLiteral("点击查看前往该站的路线"));

    distLabel->setStyleSheet(R"(
        QPushButton {
            background-color: #142C46;
            color: #7DD3FC;
            border: 1px solid #264B70;
            border-radius: 8px;
            padding: 5px 8px;
            min-height: 20px;
            font-size: 12px;
            font-weight: bold;
        }

        QPushButton:hover {
            background-color: #1E4065;
            border-color: #60A5FA;
        }

        QPushButton:pressed {
            background-color: #28558A;
        }
    )");

    connect(
        distLabel,
        &QPushButton::clicked,
        this,
        [this]()
        {
            emit navigationRequested(m_item.id);
        }
    );

    auto *addressLabel = new QLabel(m_item.address, this);
    addressLabel->setObjectName(QStringLiteral("stationAddress"));
    addressLabel->setWordWrap(true);
    addressLabel->setTextFormat(Qt::PlainText);
    addressLabel->setTextInteractionFlags(Qt::NoTextInteraction);

    auto *priceLabel =
        new QLabel(QStringLiteral("%1 元/度").arg(m_item.price, 0, 'f', 2), this);
    priceLabel->setObjectName(QStringLiteral("priceLabel"));

    auto *freeLabel = new QLabel(
        QStringLiteral("空闲 %1/%2").arg(m_item.freeChargers).arg(m_item.totalChargers),
        this);
    // 无空闲桩时标红提示
    freeLabel->setObjectName(m_item.freeChargers > 0
                                 ? QStringLiteral("freeLabel")
                                 : QStringLiteral("freeLabelFull"));

    auto *topRow = new QHBoxLayout;
    topRow->addWidget(nameLabel, 1);
    topRow->addWidget(distLabel);

    auto *bottomRow = new QHBoxLayout;
    bottomRow->addWidget(priceLabel, 1);
    bottomRow->addWidget(freeLabel);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(8);
    layout->addLayout(topRow);
    layout->addWidget(addressLabel);
    layout->addLayout(bottomRow);
}

void StationCardWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
        emit clicked(m_item.id);
        event->accept();
        return;
    }
    QFrame::mouseReleaseEvent(event);
}

} // namespace client_user
