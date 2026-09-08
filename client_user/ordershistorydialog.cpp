#include "ordershistorydialog.h"

#include <QDateTime>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

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
    setWindowTitle(QStringLiteral("我的订单"));
    resize(560, 640);
    setMinimumSize(520, 520);

    setStyleSheet(R"(
QLabel {
    color: #E5EFFF;
    font-size: 13px;
}
QLabel#historyTitle {
    font-size: 20px;
    font-weight: bold;
    color: #FFFFFF;
}
QPushButton {
    background: #2563EB;
    color: white;
    border: none;
    border-radius: 8px;
    padding: 8px 14px;
    font-size: 13px;
    min-height: 16px;
}
QPushButton:hover {
    background: #3B82F6;
}
QTableWidget {
    background: #0E1F33;
    alternate-background-color: #12283F;
    color: #E5EFFF;
    border: 1px solid #264B70;
    border-radius: 8px;
    gridline-color: #1E4065;
    font-size: 12px;
}
QTableWidget::item {
    padding: 4px;
}
QHeaderView::section {
    background: #142C46;
    color: #7DD3FC;
    border: none;
    border-bottom: 1px solid #264B70;
    padding: 6px;
    font-size: 12px;
    font-weight: bold;
}
QTableCornerButton::section {
    background: #142C46;
    border: none;
}
    )");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("我的订单"), this);
    title->setObjectName(QStringLiteral("historyTitle"));
    layout->addWidget(title);

    m_table = new QTableWidget(0, 7, this);
    m_table->setHorizontalHeaderLabels(QStringList{
        QStringLiteral("电站名"),
        QStringLiteral("电桩编号"),
        QStringLiteral("开始时间"),
        QStringLiteral("结束时间"),
        QStringLiteral("电量(度)"),
        QStringLiteral("金额(元)"),
        QStringLiteral("状态")});

    m_table->setSelectionBehavior(
        QAbstractItemView::SelectRows);
    m_table->setSelectionMode(
        QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(
        3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(
        4, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(
        5, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(
        6, QHeaderView::ResizeToContents);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(true);
    m_table->setWordWrap(false);
    layout->addWidget(m_table, 1);

    m_emptyLabel = new QLabel(
        QStringLiteral("暂无订单记录"), this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->hide();
    layout->addWidget(m_emptyLabel);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(10);

    m_receiptButton = new QPushButton(
        QStringLiteral("查看小票"), this);
    m_receiptButton->setEnabled(false);
    buttonRow->addWidget(m_receiptButton);

    m_refreshButton = new QPushButton(
        QStringLiteral("刷新"), this);
    buttonRow->addWidget(m_refreshButton);

    buttonRow->addStretch();

    m_closeButton = new QPushButton(
        QStringLiteral("关闭"), this);
    buttonRow->addWidget(m_closeButton);

    layout->addLayout(buttonRow);

    connect(
        m_receiptButton,
        &QPushButton::clicked,
        this,
        &OrderHistoryDialog::showSelectedReceipt);

    connect(
        m_refreshButton,
        &QPushButton::clicked,
        this,
        &OrderHistoryDialog::loadOrders);

    connect(
        m_closeButton,
        &QPushButton::clicked,
        this,
        &QDialog::accept);

    connect(
        m_table,
        &QTableWidget::cellDoubleClicked,
        this,
        &OrderHistoryDialog::onRowDoubleClicked);

    connect(
        m_table,
        &QTableWidget::itemSelectionChanged,
        this,
        [this]() {
            const bool hasSelection =
                m_table->currentRow() >= 0;
            m_receiptButton->setEnabled(hasSelection);
        });
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

        auto *nameItem = new QTableWidgetItem(item.stationName);
        auto *noItem = new QTableWidgetItem(item.chargerNo);
        auto *startItem = new QTableWidgetItem(startText);
        auto *endItem = new QTableWidgetItem(endText);
        auto *energyItem = new QTableWidgetItem(
            QString::number(item.energy, 'f', 2));
        auto *amountItem = new QTableWidgetItem(
            QString::number(item.amount, 'f', 2));
        auto *statusItem = new QTableWidgetItem(
            historyStatusText(item.status));

        energyItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        amountItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        m_table->setItem(row, 0, nameItem);
        m_table->setItem(row, 1, noItem);
        m_table->setItem(row, 2, startItem);
        m_table->setItem(row, 3, endItem);
        m_table->setItem(row, 4, energyItem);
        m_table->setItem(row, 5, amountItem);
        m_table->setItem(row, 6, statusItem);
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

    const bool charging = (receipt.status == 1);

    QString text = QStringLiteral(
        "订单号：%1\n"
        "电站：%2\n"
        "电桩编号：%3\n"
        "开始时间：%4\n"
        "结束时间：%5\n"
        "充电时长：%6\n"
        "电量：%7 度\n"
        "单价：%8 元/度\n"
        "%9：%10 元")
        .arg(receipt.orderId)
        .arg(receipt.stationName.isEmpty()
                 ? QStringLiteral("—")
                 : receipt.stationName)
        .arg(receipt.chargerNo.isEmpty()
                 ? QStringLiteral("—")
                 : receipt.chargerNo)
        .arg(receipt.startTime.isEmpty()
                 ? QStringLiteral("—")
                 : receipt.startTime)
        .arg(receipt.endTime.isEmpty()
                 ? QStringLiteral("—")
                 : receipt.endTime)
        .arg(historyDuration(receipt.simulatedSeconds))
        .arg(receipt.energy, 0, 'f', 2)
        .arg(receipt.unitPrice, 0, 'f', 2)
        .arg(charging
                 ? QStringLiteral("当前费用")
                 : QStringLiteral("总金额"))
        .arg(receipt.amount, 0, 'f', 2);

    text += QStringLiteral("\n状态：%1")
        .arg(historyStatusText(receipt.status));

    if (receipt.paidAmount >= 0) {
        text += QStringLiteral(
            "\n实际扣款：%1 元"
            "\n扣款后余额：%2 元")
            .arg(receipt.paidAmount, 0, 'f', 2)
            .arg(receipt.balanceAfter, 0, 'f', 2);

        if (receipt.debtAmount > 0) {
            text += QStringLiteral(
                "\n本次欠费：%1 元（请及时充值）")
                .arg(receipt.debtAmount, 0, 'f', 2);
        }
    } else if (charging) {
        text += QStringLiteral(
            "\n扣款后余额：结算时确定");
    }

    QMessageBox receiptBox(this);
    receiptBox.setWindowTitle(QStringLiteral("订单小票"));
    receiptBox.setIcon(QMessageBox::Information);
    receiptBox.setTextFormat(Qt::PlainText);
    receiptBox.setText(text);
    receiptBox.setStandardButtons(QMessageBox::Ok);
    receiptBox.setButtonText(
        QMessageBox::Ok,
        QStringLiteral("确定"));

    receiptBox.exec();
}
