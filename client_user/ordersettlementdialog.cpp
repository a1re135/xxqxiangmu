#include "ordersettlementdialog.h"

#include <QDateTime>
#include <QFrame>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "service/userservice.h"

namespace {

QString formatDuration(qint64 seconds)
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

QString statusTextOf(int status)
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
        return QStringLiteral("未知状态（%1）").arg(status);
    }
}

} // namespace

OrderSettlementDialog::OrderSettlementDialog(
    int orderId,
    int userId,
    QWidget *parent)
    : QDialog(parent)
    , m_orderId(orderId)
    , m_userId(userId)
{
    buildUi();

    reloadOrder();

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(1000);

    connect(
        m_refreshTimer,
        &QTimer::timeout,
        this,
        &OrderSettlementDialog::reloadOrder);

    m_refreshTimer->start();
}

void OrderSettlementDialog::buildUi()
{
    setWindowTitle(QStringLiteral("订单结算"));
    // 放大弹窗并允许内容滚动，保证下方信息完整可见。
    resize(440, 700);
    setMinimumSize(420, 620);

    setStyleSheet(R"(
QLabel {
    color: #E5EFFF;
    font-size: 13px;
}
QLabel#dialogTitle {
    font-size: 20px;
    font-weight: bold;
    color: #FFFFFF;
}
QLabel#socLabel {
    color: #7DD3FC;
    font-size: 12px;
}
QProgressBar {
    border: 1px solid #264B70;
    border-radius: 6px;
    background: #0E1F33;
    text-align: center;
    color: #E5EFFF;
    font-size: 11px;
    min-height: 14px;
}
QProgressBar::chunk {
    border-radius: 6px;
    background: #22C55E;
}
QPushButton {
    background: #2563EB;
    color: white;
    border: none;
    border-radius: 8px;
    padding: 10px;
    font-size: 13px;
    min-height: 18px;
}
QPushButton:hover {
    background: #3B82F6;
}
QPushButton:disabled {
    background: #263449;
    color: #8190A5;
}
QPushButton#dangerButton {
    background: #DC2626;
}
QPushButton#dangerButton:hover {
    background: #EF4444;
}
QPushButton#doneButton {
    background: #22C55E;
}
QPushButton#doneButton:hover {
    background: #4ADE80;
}
    )");

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(18, 16, 18, 16);
    outerLayout->setSpacing(12);

    m_title = new QLabel(QStringLiteral("充电订单"), this);
    m_title->setObjectName(QStringLiteral("dialogTitle"));
    outerLayout->addWidget(m_title);

    // 内容放入滚动区：信息超长时也能滚动查看，不再被裁掉。
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff);

    m_content = new QWidget(m_scroll);
    auto *contentLayout = new QVBoxLayout(m_content);
    contentLayout->setContentsMargins(2, 2, 6, 2);
    contentLayout->setSpacing(8);

    m_details = new QLabel(m_content);
    m_details->setWordWrap(true);
    m_details->setTextFormat(Qt::PlainText);
    m_details->setTextInteractionFlags(Qt::TextSelectableByMouse);
    contentLayout->addWidget(m_details);

    m_socLabel = new QLabel(QStringLiteral("模拟电量"), m_content);
    m_socLabel->setObjectName(QStringLiteral("socLabel"));
    m_socLabel->hide();
    contentLayout->addWidget(m_socLabel);

    m_socBar = new QProgressBar(m_content);
    m_socBar->setRange(0, 100);
    m_socBar->setValue(0);
    m_socBar->setTextVisible(true);
    m_socBar->setFormat(QStringLiteral("%p%"));
    m_socBar->hide();
    contentLayout->addWidget(m_socBar);

    m_notice = new QLabel(m_content);
    m_notice->setWordWrap(true);
    m_notice->setTextFormat(Qt::PlainText);
    m_notice->setStyleSheet(QStringLiteral("color: #FB923C;"));
    contentLayout->addWidget(m_notice);

    contentLayout->addStretch();

    m_scroll->setWidget(m_content);
    outerLayout->addWidget(m_scroll, 1);

    m_refreshButton = new QPushButton(
        QStringLiteral("刷新订单"), this);
    outerLayout->addWidget(m_refreshButton);

    m_cancelButton = new QPushButton(
        QStringLiteral("取消预约"), this);
    m_cancelButton->setEnabled(false);
    outerLayout->addWidget(m_cancelButton);

    m_startButton = new QPushButton(
        QStringLiteral("开始充电"), this);
    m_startButton->setEnabled(false);
    outerLayout->addWidget(m_startButton);

    m_finishButton = new QPushButton(
        QStringLiteral("结束充电"), this);
    m_finishButton->setObjectName(
        QStringLiteral("dangerButton"));
    m_finishButton->setEnabled(false);
    m_finishButton->hide();
    outerLayout->addWidget(m_finishButton);

    m_doneButton = new QPushButton(
        QStringLiteral("完成"), this);
    m_doneButton->setObjectName(QStringLiteral("doneButton"));
    m_doneButton->hide();
    outerLayout->addWidget(m_doneButton);

    connect(
        m_refreshButton,
        &QPushButton::clicked,
        this,
        &OrderSettlementDialog::reloadOrder);

    connect(
        m_cancelButton,
        &QPushButton::clicked,
        this,
        &OrderSettlementDialog::onCancelReservation);

    connect(
        m_startButton,
        &QPushButton::clicked,
        this,
        &OrderSettlementDialog::onStartCharging);

    connect(
        m_finishButton,
        &QPushButton::clicked,
        this,
        &OrderSettlementDialog::onFinishCharging);

    connect(
        m_doneButton,
        &QPushButton::clicked,
        this,
        &QDialog::accept);
}

void OrderSettlementDialog::reloadOrder()
{
    if (m_settledShown) {
        return;
    }

    core::ChargeService chargeService;
    core::ChargeOrderInfo info;
    QString errorMessage;

    if (!chargeService.getOrderInfo(
            m_userId, m_orderId, info, errorMessage)) {
        m_details->clear();
        m_notice->setText(errorMessage);
        setButtonsVisible(false, false, false);
        return;
    }

    // 预约超时自动取消；若刚取消成功则重新读取最新状态。
    QString autoCancelMessage;

    if (tryAutoCancelExpired(info, autoCancelMessage)) {
        if (!chargeService.getOrderInfo(
                m_userId, m_orderId, info, errorMessage)) {
            m_details->clear();
            m_notice->setText(errorMessage);
            setButtonsVisible(false, false, false);
            return;
        }
    }

    const QString startText = info.startTime.isEmpty()
        ? QStringLiteral("尚未开始")
        : info.startTime;

    QString text = QStringLiteral(
        "订单编号：%1\n"
        "电站：%2\n"
        "电桩编号：%3\n"
        "订单状态：%4\n"
        "开始时间：%5"
    )
        .arg(info.id)
        .arg(info.stationName)
        .arg(info.chargerNo)
        .arg(statusTextOf(info.status))
        .arg(startText);

    if (info.status == 0) {
        text += QStringLiteral(
            "\n预约保留至：%1"
            "\n超时未开始将自动取消并释放电桩。")
            .arg(info.reservationExpiresAt.isEmpty()
                     ? QStringLiteral("—")
                     : info.reservationExpiresAt);
    }

    if (info.status == 1) {
        text += QStringLiteral(
            "\n模拟充电时长：%1"
            "\n模拟功率：%2 kW"
            "\n单价：%3 元/度")
            .arg(formatDuration(info.simulatedSeconds))
            .arg(info.power, 0, 'f', 1)
            .arg(info.price, 0, 'f', 2);
    }

    text += QStringLiteral(
        "\n累计电量：%1 度"
        "\n%2：%3 元")
        .arg(info.energy, 0, 'f', 2)
        .arg(info.status == 1
                 ? QStringLiteral("当前费用")
                 : QStringLiteral("订单金额"))
        .arg(info.amount, 0, 'f', 2);

    m_details->setText(text);

    // 充电中显示模拟 SoC 进度条（按 1 模拟小时充满估算）。
    const bool isCharging = (info.status == 1);
    m_socLabel->setVisible(isCharging);
    m_socBar->setVisible(isCharging);

    if (isCharging) {
        const int percent = qBound(
            0,
            static_cast<int>(info.simulatedSeconds * 100 / 3600),
            100);
        m_socBar->setValue(percent);
        m_socLabel->setText(QStringLiteral(
            "模拟电量：%1%（按 1 小时充满估算）")
            .arg(percent));
    }

    const bool isReserved = (info.status == 0);
    const bool isSettled = (info.status == 2);

    setButtonsVisible(isReserved, isCharging, isSettled);

    switch (info.status) {
    case 0:
        m_notice->setText(QStringLiteral(
            "已预约，可开始充电或取消预约。"));
        break;
    case 1:
        m_notice->setText(QStringLiteral(
            "正在模拟充电，费用每秒刷新。"
            "点击“结束充电”后结算。"));
        break;
    case 2:
        m_notice->setText(QStringLiteral("该订单已结算。"));
        break;
    case 3:
        m_notice->setText(QStringLiteral("预约已取消。"));
        break;
    default:
        m_notice->setText(QStringLiteral("订单状态异常。"));
        break;
    }

    if (isSettled && !m_settledShown) {
        // 打开到已结算订单时，直接展示小票。
        core::OrderReceipt receipt;
        if (chargeService.getReceipt(
                m_userId, m_orderId, receipt, errorMessage)) {
            showReceipt(receipt);
        }
    }

    // 超时自动取消/失败提示优先展示。
    if (!autoCancelMessage.isEmpty()) {
        m_notice->setText(autoCancelMessage);
    }
}

bool OrderSettlementDialog::tryAutoCancelExpired(
    const core::ChargeOrderInfo &info,
    QString &message)
{
    message.clear();

    if (m_expiredHandled || info.status != 0) {
        return false;
    }

    if (info.reservationExpiresAt.isEmpty()) {
        return false;
    }

    const QDateTime expires = QDateTime::fromString(
        info.reservationExpiresAt, "yyyy-MM-dd HH:mm:ss");

    if (!expires.isValid()
        || QDateTime::currentDateTime() < expires) {
        return false;
    }

    m_expiredHandled = true;

    UserService userService;
    QString errorMessage;

    if (userService.cancelReservation(
            m_userId, m_orderId, errorMessage)) {
        message = QStringLiteral(
            "预约已超时，订单已自动取消，电桩已释放。");
    } else {
        message = QStringLiteral(
            "预约已超时，自动取消失败：%1")
            .arg(errorMessage);
    }

    return true;
}

void OrderSettlementDialog::setButtonsVisible(
    bool reserved,
    bool charging,
    bool settled)
{
    m_cancelButton->setVisible(reserved);
    m_cancelButton->setEnabled(reserved);

    m_startButton->setVisible(reserved);
    m_startButton->setEnabled(reserved);

    m_finishButton->setVisible(charging);
    m_finishButton->setEnabled(charging);

    m_doneButton->setVisible(settled);
    m_doneButton->setEnabled(settled);
}

void OrderSettlementDialog::onCancelReservation()
{
    const auto answer = QMessageBox::question(
        this,
        QStringLiteral("取消预约"),
        QStringLiteral("确定取消这笔预约吗？取消不会扣费。"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (answer != QMessageBox::Yes) {
        return;
    }

    m_cancelButton->setEnabled(false);

    UserService userService;
    QString errorMessage;

    const bool success = userService.cancelReservation(
        m_userId, m_orderId, errorMessage);

    if (!success) {
        m_notice->setText(
            QStringLiteral("取消预约失败：%1").arg(errorMessage));
        m_cancelButton->setEnabled(true);
        return;
    }

    m_notice->setText(
        QStringLiteral("预约已取消，电桩已释放。"));
    accept();
}

void OrderSettlementDialog::onStartCharging()
{
    m_startButton->setEnabled(false);

    core::ChargeService chargeService;
    QString errorMessage;

    const bool success = chargeService.startReservedCharging(
        m_userId, m_orderId, errorMessage);

    reloadOrder();

    if (!success) {
        QMessageBox::warning(
            this,
            QStringLiteral("无法开始充电"),
            errorMessage);
        return;
    }

    QMessageBox::information(
        this,
        QStringLiteral("开始成功"),
        QStringLiteral(
            "订单已进入充电中状态，开始时间已保存。"
            "\n费用与模拟电量将每秒实时刷新。"));
}

void OrderSettlementDialog::onFinishCharging()
{
    const auto answer = QMessageBox::question(
        this,
        QStringLiteral("结束充电"),
        QStringLiteral(
            "确定结束充电并结算吗？\n"
            "费用按确认后的结束时间计算。\n"
            "余额不足时将扣至0，并记录剩余欠费。"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (answer != QMessageBox::Yes) {
        return;
    }

    m_finishButton->setEnabled(false);

    core::ChargeService chargeService;
    core::SettlementResult result;
    QString errorMessage;

    const bool success = chargeService.finishCharging(
        m_userId,
        m_orderId,
        result,
        errorMessage);

    if (!success) {
        // 任一步失败则整体回滚，订单保持充电中状态。
        reloadOrder();
        QMessageBox::warning(
            this,
            QStringLiteral("结算失败，请重试"),
            errorMessage);
        return;
    }

    // 结算成功：读取完整小票并切换到结算页。
    core::OrderReceipt receipt;

    if (!chargeService.getReceipt(
            m_userId, m_orderId, receipt, errorMessage)) {
        receipt.orderId = result.orderId;
        receipt.status = 2;
        receipt.endTime = result.endTime;
        receipt.simulatedSeconds = result.simulatedSeconds;
        receipt.energy = result.energy;
        receipt.amount = result.amount;
        receipt.paidAmount = result.paidAmount;
        receipt.debtAmount = result.debtAmount;
        receipt.balanceAfter = result.balanceAfter;
    }

    showReceipt(receipt);
}

void OrderSettlementDialog::showReceipt(
    const core::OrderReceipt &receipt)
{
    m_settledShown = true;

    if (m_refreshTimer) {
        m_refreshTimer->stop();
    }

    m_title->setText(QStringLiteral("结算小票"));

    QString text = QStringLiteral(
        "订单号：%1\n"
        "电站：%2\n"
        "电桩编号：%3\n"
        "开始时间：%4\n"
        "结束时间：%5\n"
        "充电时长：%6\n"
        "电量：%7 度\n"
        "单价：%8 元/度\n"
        "总金额：%9 元")
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
        .arg(formatDuration(receipt.simulatedSeconds))
        .arg(receipt.energy, 0, 'f', 2)
        .arg(receipt.unitPrice, 0, 'f', 2)
        .arg(receipt.amount, 0, 'f', 2);

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
    } else {
        text += QStringLiteral(
            "\n扣款后余额：结算时确定");
    }

    m_details->setText(text);
    m_details->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    m_socLabel->hide();
    m_socBar->hide();
    m_notice->setText(QStringLiteral(
        "充电已完成，电桩已恢复空闲，费用已从余额扣除。"));

    setButtonsVisible(false, false, true);
}
