#include "ordershistorydialog.h"

#include <QDateTime>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>

#include "service/charge_service.h"

namespace {

QString historyStatusText(int status)
{
    switch (status) {
    case 0:
        return QStringLiteral("预约中");
    case 1:
        return QStringLiteral("充电中");
    case 2:
        return QStringLiteral("已结算");
    case 3:
        return QStringLiteral("已取消");
    default:
        return QStringLiteral("未知");
    }
}

QString historyDuration(qint64 seconds)
{
    if (seconds < 0) {
        seconds = 0;
    }

    // 不使用 QTime，避免超过 24 小时后从零开始显示。
    return QStringLiteral("%1:%2:%3")
        .arg(seconds / 3600, 2, 10, QChar('0'))
        .arg((seconds / 60) % 60, 2, 10, QChar('0'))
        .arg(seconds % 60, 2, 10, QChar('0'));
}

} // namespace

OrderHistoryDialog::OrderHistoryDialog(
    int userId,
    QWidget *parent)
    : QDialog(parent)
    , m_userId(userId)
{
    buildUi();
    loadOrders();
}

void OrderHistoryDialog::buildUi()
{
    setWindowTitle(
        QStringLiteral("我的订单")
    );

    setFixedSize(600, 560);

    setStyleSheet(R"(

        QDialog {
            background-color: #08111F;
        }

        QLabel {
            background: transparent;
            color: #DCEBFF;
        }

        QLabel#historyTitle {
            color: #FFFFFF;
            font-size: 24px;
            font-weight: 800;
        }

        QLabel#historySubtitle {
            color: #7891AF;
            font-size: 11px;
        }

        QTableWidget {
            background-color: #0D1B2D;
            alternate-background-color: #11253D;

            color: #EDF5FF;

            border: 1px solid #294C70;
            border-radius: 12px;

            gridline-color: #213C58;

            font-size: 11px;
        }

        QTableWidget::item {
            padding: 5px;
            border-bottom: 1px solid #1D3650;
        }

        QTableWidget::item:selected {
            background-color: #22588B;
            color: #FFFFFF;
        }

        QHeaderView::section {
            background-color: #173656;
            color: #9FD0FF;

            border: none;
            border-right: 1px solid #294C70;

            padding: 8px 4px;

            font-size: 11px;
            font-weight: 700;
        }

        QTableCornerButton::section {
            background-color: #173656;
            border: none;
        }

        QPushButton {
            min-height: 36px;

            padding-left: 16px;
            padding-right: 16px;

            border-radius: 9px;

            background-color: #122840;
            color: #CDE5FF;

            border: 1px solid #315A82;

            font-size: 12px;
            font-weight: 600;
        }

        QPushButton:hover {
            background-color: #1A3A5D;
            border-color: #60A5FA;
        }

        QPushButton#receiptButton {
            background-color: #2563EB;
            color: #FFFFFF;
            border-color: #4B8CFF;
        }

        QPushButton#receiptButton:hover {
            background-color: #397EF1;
        }

        QPushButton:disabled {
            background-color: #162638;
            color: #63758A;
            border-color: #263A52;
        }

    )");


    auto *layout =
        new QVBoxLayout(this);

    layout->setContentsMargins(
        18, 18, 18, 18
    );

    layout->setSpacing(12);


    // ==========================
    // Header
    // ==========================

    auto *title =
        new QLabel(
            QStringLiteral("我的订单"),
            this
        );

    title->setObjectName(
        QStringLiteral("historyTitle")
    );

    title->setAlignment(
        Qt::AlignCenter
    );


    auto *subtitle =
        new QLabel(
            QStringLiteral(
                "查看充电记录与结算小票"
            ),
            this
        );

    subtitle->setObjectName(
        QStringLiteral("historySubtitle")
    );

    subtitle->setAlignment(
        Qt::AlignCenter
    );


    layout->addWidget(title);
    layout->addWidget(subtitle);


    // ==========================
    // Table
    // ==========================

    m_table =
        new QTableWidget(
            0,
            4,
            this
        );

    m_table->setHorizontalHeaderLabels({
        QStringLiteral("电站 / 电桩"),
        QStringLiteral("充电时间"),
        QStringLiteral("电量 / 金额"),
        QStringLiteral("状态")
    });

    m_table->setSelectionBehavior(
        QAbstractItemView::SelectRows
    );

    m_table->setSelectionMode(
        QAbstractItemView::SingleSelection
    );

    m_table->setEditTriggers(
        QAbstractItemView::NoEditTriggers
    );

    m_table->verticalHeader()
        ->setVisible(false);

    m_table->setAlternatingRowColors(true);

    m_table->setShowGrid(false);

    m_table->setWordWrap(true);

    m_table->setFocusPolicy(
        Qt::NoFocus
    );

    m_table->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff
    );

    m_table->horizontalHeader()
        ->setDefaultAlignment(
            Qt::AlignCenter
        );

    m_table->horizontalHeader()
        ->setSectionResizeMode(
            0,
            QHeaderView::Stretch
        );

    m_table->horizontalHeader()
        ->setSectionResizeMode(
            1,
            QHeaderView::Stretch
        );

    m_table->horizontalHeader()
        ->setSectionResizeMode(
            2,
            QHeaderView::ResizeToContents
        );

    m_table->horizontalHeader()
        ->setSectionResizeMode(
            3,
            QHeaderView::ResizeToContents
        );

    m_table->verticalHeader()
        ->setDefaultSectionSize(58);

    layout->addWidget(
        m_table,
        1
    );


    m_emptyLabel =
        new QLabel(
            QStringLiteral(
                "暂无订单记录"
            ),
            this
        );

    m_emptyLabel->setAlignment(
        Qt::AlignCenter
    );

    m_emptyLabel->hide();

    layout->addWidget(
        m_emptyLabel
    );


    // ==========================
    // Buttons
    // ==========================

    auto *buttonRow =
        new QHBoxLayout;

    buttonRow->setSpacing(10);


    m_receiptButton =
        new QPushButton(
            QStringLiteral(
                "查看小票"
            ),
            this
        );

    m_receiptButton->setObjectName(
        QStringLiteral(
            "receiptButton"
        )
    );

    m_receiptButton->setEnabled(false);


    m_refreshButton =
        new QPushButton(
            QStringLiteral("刷新"),
            this
        );


    m_closeButton =
        new QPushButton(
            QStringLiteral("关闭"),
            this
        );


    buttonRow->addWidget(
        m_receiptButton
    );

    buttonRow->addWidget(
        m_refreshButton
    );

    buttonRow->addStretch();

    buttonRow->addWidget(
        m_closeButton
    );


    layout->addLayout(
        buttonRow
    );


    connect(
        m_receiptButton,
        &QPushButton::clicked,
        this,
        &OrderHistoryDialog::
            showSelectedReceipt
    );

    connect(
        m_refreshButton,
        &QPushButton::clicked,
        this,
        &OrderHistoryDialog::loadOrders
    );

    connect(
        m_closeButton,
        &QPushButton::clicked,
        this,
        &QDialog::accept
    );

    connect(
        m_table,
        &QTableWidget::cellDoubleClicked,
        this,
        &OrderHistoryDialog::
            onRowDoubleClicked
    );

    connect(
        m_table,
        &QTableWidget::
            itemSelectionChanged,
        this,
        [this]()
        {
            m_receiptButton->setEnabled(
                m_table->currentRow() >= 0
            );
        }
    );
}

void OrderHistoryDialog::loadOrders()
{
    m_table->setRowCount(0);
    m_orderIds.clear();

    core::ChargeService chargeService;
    QVector<core::OrderHistoryItem> items;
    QString errorMessage;

    if (!chargeService.listOrders(
            m_userId, items, errorMessage)) {
        m_emptyLabel->setText(errorMessage);
        m_emptyLabel->show();
        return;
    }

    m_emptyLabel->setVisible(items.isEmpty());
    m_emptyLabel->setText(QStringLiteral("暂无订单记录"));

    if (items.isEmpty()) {
        return;
    }

    m_table->setRowCount(items.size());

    for (int row = 0; row < items.size(); ++row) {
        const core::OrderHistoryItem &item = items.at(row);

        m_orderIds.append(item.id);

        const QString startText = item.startTime.isEmpty()
            ? QStringLiteral("—")
            : item.startTime;
        const QString endText = item.endTime.isEmpty()
            ? QStringLiteral("—")
            : item.endTime;

        const QString stationText =
            QStringLiteral("%1\n%2")
                .arg(
                    item.stationName.isEmpty()
                        ? QStringLiteral("—")
                        : item.stationName
                )
                .arg(
                    item.chargerNo.isEmpty()
                        ? QStringLiteral("—")
                        : item.chargerNo
                );

        const QString timeText =
            QStringLiteral("%1\n%2")
                .arg(startText)
                .arg(endText);

        const QString amountText =
            QStringLiteral("%1 度\n¥ %2")
                .arg(
                    item.energy,
                    0,
                    'f',
                    2
                )
                .arg(
                    item.amount,
                    0,
                    'f',
                    2
                );

        auto *stationItem =
            new QTableWidgetItem(stationText);

        auto *timeItem =
            new QTableWidgetItem(timeText);

        auto *amountItem =
            new QTableWidgetItem(amountText);

        auto *statusItem =
            new QTableWidgetItem(
                historyStatusText(item.status)
            );

        stationItem->setTextAlignment(
            Qt::AlignCenter
        );

        timeItem->setTextAlignment(
            Qt::AlignCenter
        );

        amountItem->setTextAlignment(
            Qt::AlignCenter
        );

        statusItem->setTextAlignment(
            Qt::AlignCenter
        );

        stationItem->setToolTip(
            item.stationName
            + QStringLiteral("\n")
            + item.chargerNo
        );

        m_table->setItem(
            row,
            0,
            stationItem
        );

        m_table->setItem(
            row,
            1,
            timeItem
        );

        m_table->setItem(
            row,
            2,
            amountItem
        );

        m_table->setItem(
            row,
            3,
            statusItem
        );
    }

    m_table->selectRow(0);
}

void OrderHistoryDialog::onRowDoubleClicked(
    int row,
    int column)
{
    Q_UNUSED(column);

    m_table->selectRow(row);
    showSelectedReceipt();
}

void OrderHistoryDialog::showSelectedReceipt()
{
    const int row = m_table->currentRow();

    if (row < 0 || row >= m_orderIds.size()) {
        return;
    }

    showReceiptOf(m_orderIds.at(row));
}

void OrderHistoryDialog::showReceiptOf(int orderId)
{
    core::ChargeService chargeService;
    core::OrderReceipt receipt;
    QString errorMessage;

    if (!chargeService.getReceipt(
            m_userId, orderId, receipt, errorMessage)) {
        QMessageBox::warning(
            this,
            QStringLiteral("读取小票失败"),
            errorMessage);
        return;
    }

    const bool charging =
        (receipt.status == 1);


    QDialog dialog(this);

    dialog.setWindowTitle(
        QStringLiteral("订单小票")
    );

    dialog.setFixedSize(
        400,
        560
    );

    dialog.setStyleSheet(R"(

        QDialog {
            background-color: #08111F;
        }

        QLabel {
            background: transparent;
            color: #EAF3FF;
        }

        QLabel#receiptTitle {
            color: #FFFFFF;

            font-size: 23px;
            font-weight: 800;
        }

        QLabel#receiptSubtitle {
            color: #7891AF;
            font-size: 11px;
        }

        QFrame#receiptCard {
            background-color: #0E2037;

            border: 1px solid #295078;
            border-radius: 15px;
        }

        QLabel#rowName {
            color: #8FA9C8;
            font-size: 12px;
        }

        QLabel#rowValue {
            color: #F2F5FA;

            font-size: 12px;
            font-weight: 600;
        }

        QPushButton {
            background-color: #2563EB;

            color: white;

            border: 1px solid #4B8CFF;
            border-radius: 10px;

            min-height: 40px;

            font-size: 13px;
            font-weight: 700;
        }

        QPushButton:hover {
            background-color: #397EF1;
        }

    )");


    auto *layout =
        new QVBoxLayout(&dialog);

    layout->setContentsMargins(
        18, 18, 18, 18
    );

    layout->setSpacing(10);


    auto *title =
        new QLabel(
            QStringLiteral(
                "充电结算小票"
            ),
            &dialog
        );

    title->setObjectName(
        QStringLiteral("receiptTitle")
    );

    title->setAlignment(
        Qt::AlignCenter
    );


    auto *subtitle =
        new QLabel(
            QStringLiteral(
                "NCS CHARGE · ORDER RECEIPT"
            ),
            &dialog
        );

    subtitle->setObjectName(
        QStringLiteral(
            "receiptSubtitle"
        )
    );

    subtitle->setAlignment(
        Qt::AlignCenter
    );


    layout->addWidget(title);
    layout->addWidget(subtitle);


    auto *card =
        new QFrame(&dialog);

    card->setObjectName(
        QStringLiteral(
            "receiptCard"
        )
    );


    auto *grid =
        new QGridLayout(card);

    grid->setContentsMargins(
        16, 14, 16, 14
    );

    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(10);


    int receiptRow = 0;


    auto addRow =
        [&](const QString &name,
            const QString &value,
            const QString &valueColor =
                QStringLiteral("#F2F5FA"))
    {
        auto *nameLabel =
            new QLabel(
                name,
                card
            );

        nameLabel->setObjectName(
            QStringLiteral("rowName")
        );


        auto *valueLabel =
            new QLabel(
                value,
                card
            );

        valueLabel->setObjectName(
            QStringLiteral("rowValue")
        );

        valueLabel->setAlignment(
            Qt::AlignRight |
            Qt::AlignVCenter
        );

        valueLabel->setWordWrap(true);

        valueLabel->setStyleSheet(
            QStringLiteral(
                "color:%1;"
                "font-size:12px;"
                "font-weight:600;"
            ).arg(valueColor)
        );


        grid->addWidget(
            nameLabel,
            receiptRow,
            0
        );

        grid->addWidget(
            valueLabel,
            receiptRow,
            1
        );

        ++receiptRow;
    };


    addRow(
        QStringLiteral("订单编号"),
        QString::number(
            receipt.orderId
        )
    );

    addRow(
        QStringLiteral("电站"),
        receipt.stationName.isEmpty()
            ? QStringLiteral("—")
            : receipt.stationName
    );

    addRow(
        QStringLiteral("电桩编号"),
        receipt.chargerNo.isEmpty()
            ? QStringLiteral("—")
            : receipt.chargerNo
    );

    addRow(
        QStringLiteral("开始时间"),
        receipt.startTime.isEmpty()
            ? QStringLiteral("—")
            : receipt.startTime
    );

    addRow(
        QStringLiteral("结束时间"),
        receipt.endTime.isEmpty()
            ? QStringLiteral("—")
            : receipt.endTime
    );

    addRow(
        QStringLiteral("充电时长"),
        historyDuration(
            receipt.simulatedSeconds
        )
    );

    addRow(
        QStringLiteral("充电电量"),
        QStringLiteral("%1 度")
            .arg(
                receipt.energy,
                0,
                'f',
                2
            )
    );

    addRow(
        QStringLiteral("充电单价"),
        QStringLiteral("%1 元/度")
            .arg(
                receipt.unitPrice,
                0,
                'f',
                2
            )
    );

    addRow(
        charging
            ? QStringLiteral("当前费用")
            : QStringLiteral("订单金额"),

        QStringLiteral("¥ %1")
            .arg(
                receipt.amount,
                0,
                'f',
                2
            ),

        QStringLiteral("#60A5FA")
    );

    addRow(
        QStringLiteral("订单状态"),
        historyStatusText(
            receipt.status
        ),

        receipt.status == 2
            ? QStringLiteral("#34D399")
            : QStringLiteral("#60A5FA")
    );


    if (receipt.paidAmount >= 0) {

        addRow(
            QStringLiteral("实际扣款"),
            QStringLiteral("¥ %1")
                .arg(
                    receipt.paidAmount,
                    0,
                    'f',
                    2
                ),

            QStringLiteral("#34D399")
        );


        addRow(
            QStringLiteral("扣款后余额"),
            QStringLiteral("¥ %1")
                .arg(
                    receipt.balanceAfter,
                    0,
                    'f',
                    2
                )
        );


        if (receipt.debtAmount > 0) {

            addRow(
                QStringLiteral("本次欠费"),
                QStringLiteral("¥ %1")
                    .arg(
                        receipt.debtAmount,
                        0,
                        'f',
                        2
                    ),

                QStringLiteral("#FB7185")
            );
        }

    } else if (charging) {

        addRow(
            QStringLiteral("扣款后余额"),
            QStringLiteral("结算时确定")
        );
    }


    layout->addWidget(
        card,
        1
    );


    auto *okButton =
        new QPushButton(
            QStringLiteral("确定"),
            &dialog
        );

    layout->addWidget(
        okButton
    );


    connect(
        okButton,
        &QPushButton::clicked,
        &dialog,
        &QDialog::accept
    );


    dialog.exec();
}
