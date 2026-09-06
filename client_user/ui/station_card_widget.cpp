#include "station_card_widget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
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
    setFixedHeight(92);

    auto *nameLabel = new QLabel(m_item.name, this);
    nameLabel->setObjectName(QStringLiteral("stationName"));

    auto *distLabel = new QLabel(
        QStringLiteral("%1 km").arg(m_item.distanceKm, 0, 'f', 1), this);
    distLabel->setObjectName(QStringLiteral("distLabel"));

    auto *addressLabel = new QLabel(m_item.address, this);
    addressLabel->setObjectName(QStringLiteral("stationAddress"));
    addressLabel->setWordWrap(false);
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
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(4);
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
