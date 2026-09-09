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
    // ============================================================
    // Card
    // ============================================================

    setObjectName(
        QStringLiteral("stationCard")
    );

    setCursor(
        Qt::PointingHandCursor
    );

    // The original card was only 120px high.
    // We now have an extra AI prediction row.
    setMinimumHeight(
        m_item.hasPrediction
            ? 155
            : 125
    );

    setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Minimum
    );


    // ============================================================
    // Station name
    // ============================================================

    auto *nameLabel =
        new QLabel(
            m_item.name,
            this
        );

    nameLabel->setObjectName(
        QStringLiteral(
            "stationName"
        )
    );

    nameLabel->setTextFormat(
        Qt::PlainText
    );

    nameLabel->setWordWrap(true);

    nameLabel->setStyleSheet(
        QStringLiteral(R"(
            QLabel {
                color:#F5F7FA;
                font-size:15px;
                font-weight:800;
                background:transparent;
                border:none;
            }
        )")
    );


    // ============================================================
    // AI recommended badge
    // ============================================================

    QLabel *recommendBadge =
        nullptr;

    if (m_item.recommended) {

        recommendBadge =
            new QLabel(
                QStringLiteral(
                    "AI推荐"
                ),
                this
            );

        recommendBadge
            ->setAlignment(
                Qt::AlignCenter
            );

        recommendBadge
            ->setFixedHeight(28);

        recommendBadge
            ->setStyleSheet(
                QStringLiteral(R"(
                    QLabel {
                        background:#123F37;
                        color:#34D399;

                        border:1px solid #1F806A;
                        border-radius:7px;

                        padding:0px 9px;

                        font-size:11px;
                        font-weight:800;
                    }
                )")
            );
    }


    // ============================================================
    // Distance button
    // ============================================================

    auto *distanceButton =
        new QPushButton(
            QStringLiteral(
                "%1 km  ›"
            ).arg(
                m_item.distanceKm,
                0,
                'f',
                1
            ),
            this
        );

    distanceButton->setObjectName(
        QStringLiteral(
            "distanceButton"
        )
    );

    distanceButton->setCursor(
        Qt::PointingHandCursor
    );

    distanceButton->setToolTip(
        QStringLiteral(
            "点击查看前往该站的路线"
        )
    );

    distanceButton->setFixedHeight(
        32
    );

    distanceButton->setStyleSheet(
        QStringLiteral(R"(
            QPushButton {
                background:#142C46;
                color:#7DD3FC;

                border:1px solid #264B70;
                border-radius:8px;

                padding:4px 9px;

                font-size:12px;
                font-weight:700;
            }

            QPushButton:hover {
                background:#1E4065;
                border-color:#60A5FA;
                color:#FFFFFF;
            }

            QPushButton:pressed {
                background:#28558A;
            }
        )")
    );


    connect(
        distanceButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            emit navigationRequested(
                m_item.id
            );
        }
    );


    // ============================================================
    // Top row
    //
    // Station name        AI推荐      distance
    // ============================================================

    auto *topRow =
        new QHBoxLayout;

    topRow->setContentsMargins(
        0, 0, 0, 0
    );

    topRow->setSpacing(8);

    topRow->addWidget(
        nameLabel,
        1
    );

    if (recommendBadge) {
        topRow->addWidget(
            recommendBadge,
            0,
            Qt::AlignTop
        );
    }

    topRow->addWidget(
        distanceButton,
        0,
        Qt::AlignTop
    );


    // ============================================================
    // Address
    // ============================================================

    auto *addressLabel =
        new QLabel(
            m_item.address,
            this
        );

    addressLabel->setObjectName(
        QStringLiteral(
            "stationAddress"
        )
    );

    addressLabel->setTextFormat(
        Qt::PlainText
    );

    addressLabel->setWordWrap(false);

    addressLabel->setStyleSheet(
        QStringLiteral(R"(
            QLabel {
                color:#7F95AC;
                background:transparent;
                border:none;

                font-size:11px;
                font-weight:500;
            }
        )")
    );


    // ============================================================
    // AI prediction
    // ============================================================

    QLabel *predictionInfoLabel =
        nullptr;

    if (m_item.hasPrediction) {

        QString predictionText =
            QStringLiteral(
                "AI预测  ·  空闲 %1/%2"
                "  ·  负荷 %3 kWh"
            )
                .arg(
                    m_item
                        .predictedFreeChargers
                )
                .arg(
                    m_item.totalChargers
                )
                .arg(
                    m_item.predictedLoad,
                    0,
                    'f',
                    2
                );


        if (m_item.predictedPeak) {
            predictionText +=
                QStringLiteral(
                    "  ·  高峰预警"
                );
        }


        predictionInfoLabel =
            new QLabel(
                predictionText,
                this
            );

        predictionInfoLabel
            ->setTextFormat(
                Qt::PlainText
            );

        predictionInfoLabel
            ->setWordWrap(false);


        if (m_item.predictedPeak) {

            predictionInfoLabel
                ->setStyleSheet(
                    QStringLiteral(R"(
                        QLabel {
                            color:#F87171;
                            background:transparent;
                            border:none;

                            font-size:11px;
                            font-weight:700;
                        }
                    )")
                );

        } else {

            predictionInfoLabel
                ->setStyleSheet(
                    QStringLiteral(R"(
                        QLabel {
                            color:#34D399;
                            background:transparent;
                            border:none;

                            font-size:11px;
                            font-weight:700;
                        }
                    )")
                );
        }
    }


    // ============================================================
    // Price
    // ============================================================

    auto *priceLabel =
        new QLabel(
            QStringLiteral(
                "%1 元/度"
            ).arg(
                m_item.price,
                0,
                'f',
                2
            ),
            this
        );

    priceLabel->setObjectName(
        QStringLiteral(
            "priceLabel"
        )
    );

    priceLabel->setStyleSheet(
        QStringLiteral(R"(
            QLabel {
                color:#60A5FA;
                background:transparent;
                border:none;

                font-size:16px;
                font-weight:800;
            }
        )")
    );


    // ============================================================
    // Current free chargers
    // ============================================================

    auto *freeLabel =
        new QLabel(
            QStringLiteral(
                "空闲 %1/%2"
            )
                .arg(
                    m_item.freeChargers
                )
                .arg(
                    m_item.totalChargers
                ),
            this
        );

    freeLabel->setAlignment(
        Qt::AlignCenter
    );

    freeLabel->setFixedHeight(
        27
    );


    if (m_item.freeChargers > 0) {

        freeLabel->setStyleSheet(
            QStringLiteral(R"(
                QLabel {
                    background:#0C3C35;
                    color:#34D399;

                    border:1px solid #106B58;
                    border-radius:7px;

                    padding:0px 9px;

                    font-size:11px;
                    font-weight:800;
                }
            )")
        );

    } else {

        freeLabel->setStyleSheet(
            QStringLiteral(R"(
                QLabel {
                    background:#40232B;
                    color:#F87171;

                    border:1px solid #7F3342;
                    border-radius:7px;

                    padding:0px 9px;

                    font-size:11px;
                    font-weight:800;
                }
            )")
        );
    }


    // ============================================================
    // Bottom row
    // ============================================================

    auto *bottomRow =
        new QHBoxLayout;

    bottomRow->setContentsMargins(
        0, 0, 0, 0
    );

    bottomRow->setSpacing(8);

    bottomRow->addWidget(
        priceLabel
    );

    bottomRow->addStretch();

    bottomRow->addWidget(
        freeLabel
    );


    // ============================================================
    // Main layout
    // ============================================================

    auto *cardLayout =
        new QVBoxLayout(this);

    cardLayout->setContentsMargins(
        14,
        12,
        14,
        12
    );

    cardLayout->setSpacing(
        6
    );

    cardLayout->addLayout(
        topRow
    );

    cardLayout->addWidget(
        addressLabel
    );


    if (predictionInfoLabel) {

        cardLayout->addWidget(
            predictionInfoLabel
        );
    }


    cardLayout->addStretch();

    cardLayout->addLayout(
        bottomRow
    );
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
