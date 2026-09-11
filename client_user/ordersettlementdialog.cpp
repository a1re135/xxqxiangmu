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

constexpr qint64 kFullChargeSimulatedSeconds = 3600;

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

QString statusColorOf(int status)
{
    switch (status) {
    case 0:
        return QStringLiteral("#60A5FA");

    case 1:
        return QStringLiteral("#22C55E");

    case 2:
        return QStringLiteral("#34D399");

    case 3:
        return QStringLiteral("#FB7185");

    default:
        return QStringLiteral("#94A3B8");
    }
}


QString orderRowHtml(
    const QString &label,
    const QString &value,
    const QString &valueColor =
        QStringLiteral("#F2F5FA"))
{
    return QStringLiteral(
        "<tr>"
        "<td style=\""
        "padding:9px 4px;"
        "color:#8FA9C8;"
        "font-size:12px;"
        "\">"
        "%1"
        "</td>"

        "<td align=\"right\" style=\""
        "padding:9px 4px;"
        "color:%3;"
        "font-size:13px;"
        "font-weight:600;"
        "\">"
        "%2"
        "</td>"
        "</tr>"
    )
        .arg(label.toHtmlEscaped())
        .arg(value.toHtmlEscaped())
        .arg(valueColor);
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
    setWindowTitle(
            QStringLiteral("订单结算")
        );

   setFixedSize(420, 720);

   setStyleSheet(R"(

       QDialog {
           background-color: #08111F;
       }

       QLabel {
           color: #EAF3FF;
       }

       QLabel#orderBrand {
           color: #60A5FA;

           font-size: 12px;
           font-weight: 700;

           letter-spacing: 1px;
       }

       QLabel#dialogTitle {
           color: #FFFFFF;

           font-size: 25px;
           font-weight: 800;
       }

       QLabel#dialogSubtitle {
           color: #7E9AB8;

           font-size: 11px;
       }

       QLabel#detailsCard {
           background-color: #0E2037;

           color: #F2F5FA;

           border: 1px solid #295078;
           border-radius: 15px;

           padding: 14px;
       }

       QLabel#socLabel {
           color: #8FC8FF;

           font-size: 12px;
           font-weight: 600;
       }

       QLabel#orderNotice {
           background-color: #2C2119;

           color: #FB923C;

           border: 1px solid #7A4821;
           border-radius: 11px;

           padding: 11px;

           font-size: 12px;
           font-weight: 600;
       }


       QScrollArea {
           background: transparent;
           border: none;
       }

       QWidget#orderContent {
           background-color: #08111F;
       }


       QProgressBar {
           background-color: #0D1B2E;

           border: 1px solid #294C70;
           border-radius: 7px;

           color: #DCEBFF;

           text-align: center;

           min-height: 16px;
       }

       QProgressBar::chunk {
           background:
               qlineargradient(
                   x1:0, y1:0,
                   x2:1, y2:0,
                   stop:0 #2563EB,
                   stop:1 #22C55E
               );

           border-radius: 6px;
       }


       QPushButton {
           min-height: 42px;

           border-radius: 10px;

           font-size: 14px;
           font-weight: 700;
       }


       QPushButton#refreshButton {
           background-color: #173A62;

           color: #DCEBFF;

           border: 1px solid #34699B;
       }

       QPushButton#refreshButton:hover {
           background-color: #215083;
       }


       QPushButton#cancelButton {
           background-color: #10233B;

           color: #78B9FF;

           border: 1px solid #3479BE;
       }

       QPushButton#cancelButton:hover {
           background-color: #173B60;
       }


       QPushButton#startButton {
           background:
               qlineargradient(
                   x1:0, y1:0,
                   x2:1, y2:0,
                   stop:0 #2563EB,
                   stop:1 #2688FF
               );

           color: white;

           border: 1px solid #4B91FF;
       }

       QPushButton#startButton:hover {
           background-color: #397EF1;
       }


       QPushButton#dangerButton {
           background-color: #B91C1C;

           color: white;

           border: 1px solid #EF4444;
       }

       QPushButton#dangerButton:hover {
           background-color: #DC2626;
       }


       QPushButton#doneButton {
           background-color: #059669;

           color: white;

           border: 1px solid #34D399;
       }

       QPushButton#doneButton:hover {
           background-color: #10B981;
       }


       QPushButton:disabled {
           background-color: #172638;

           color: #64748B;

           border: 1px solid #263A52;
       }

   )");


   auto *outerLayout =
       new QVBoxLayout(this);

   outerLayout->setContentsMargins(
       16, 14, 16, 16
   );

   outerLayout->setSpacing(10);


   // ==============================
   // Header
   // ==============================

   auto *brand =
       new QLabel(
           QStringLiteral(
               "⚡  NCS CHARGE"
           ),
           this
       );

   brand->setObjectName(
       QStringLiteral("orderBrand")
   );

   brand->setAlignment(
       Qt::AlignCenter
   );


   m_title =
       new QLabel(
           QStringLiteral("充电订单"),
           this
       );

   m_title->setObjectName(
       QStringLiteral("dialogTitle")
   );

   m_title->setAlignment(
       Qt::AlignCenter
   );


   auto *subtitle =
       new QLabel(
           QStringLiteral(
               "SMART CHARGING PLATFORM"
           ),
           this
       );

   subtitle->setObjectName(
       QStringLiteral(
           "dialogSubtitle"
       )
   );

   subtitle->setAlignment(
       Qt::AlignCenter
   );


   outerLayout->addWidget(brand);
   outerLayout->addWidget(m_title);
   outerLayout->addWidget(subtitle);

   outerLayout->addSpacing(4);


   // ==============================
   // Scroll area
   // ==============================

   m_scroll = new QScrollArea(this);

   m_scroll->setWidgetResizable(true);

   m_scroll->setFrameShape(
       QFrame::NoFrame
   );

   m_scroll->setHorizontalScrollBarPolicy(
       Qt::ScrollBarAlwaysOff
   );


   m_content =
       new QWidget(m_scroll);

   m_content->setObjectName(
       QStringLiteral(
           "orderContent"
       )
   );


   auto *contentLayout =
       new QVBoxLayout(m_content);

   contentLayout->setContentsMargins(
       1, 1, 5, 1
   );

   contentLayout->setSpacing(10);


   m_details =
       new QLabel(m_content);

   m_details->setObjectName(
       QStringLiteral(
           "detailsCard"
       )
   );

   m_details->setWordWrap(true);

   m_details->setTextFormat(
       Qt::RichText
   );

   m_details->setTextInteractionFlags(
       Qt::TextSelectableByMouse
   );

   m_details->setAlignment(
       Qt::AlignTop
   );

   contentLayout->addWidget(
       m_details
   );

   m_liveDetails = new QLabel(m_content);
   m_liveDetails->setObjectName(QStringLiteral("liveDetails"));
   m_liveDetails->setWordWrap(true);
   m_liveDetails->setTextFormat(Qt::PlainText);
   m_liveDetails->setStyleSheet(
       QStringLiteral(
           "QLabel#liveDetails {"
           "color:#EAF3FF;"
           "font-size:15px;"
           "padding:8px 0;"
           "}"
       )
   );


   // ==============================
   // Charging progress
   // ==============================

   m_socLabel =
       new QLabel(
           QStringLiteral("模拟电量"),
           m_content
       );

   m_socLabel->setObjectName(
       QStringLiteral("socLabel")
   );

   m_socLabel->hide();

   contentLayout->addWidget(
       m_socLabel
   );


   m_socBar =
       new QProgressBar(m_content);

   m_socBar->setRange(0, 100);
   m_socBar->setValue(0);
   m_socBar->setTextVisible(true);

   m_socBar->setFormat(
       QStringLiteral("%p%")
   );

   m_socBar->hide();

   contentLayout->addWidget(
       m_socBar
   );


   // ==============================
   // Notice
   // ==============================

   m_notice =
       new QLabel(m_content);

   m_notice->setObjectName(
       QStringLiteral(
           "orderNotice"
       )
   );

   m_notice->setWordWrap(true);

   m_notice->setAlignment(
       Qt::AlignCenter
   );

   contentLayout->addWidget(
       m_notice
   );


   contentLayout->addStretch();

   m_scroll->setWidget(
       m_content
   );

   outerLayout->addWidget(
       m_scroll,
       1
   );


   // ==============================
   // Action buttons
   // ==============================

   m_refreshButton =
       new QPushButton(
           QStringLiteral(
               "↻  刷新订单"
           ),
           this
       );

   m_refreshButton->setObjectName(
       QStringLiteral(
           "refreshButton"
       )
   );


   m_cancelButton =
       new QPushButton(
           QStringLiteral(
               "✕  取消预约"
           ),
           this
       );

   m_cancelButton->setObjectName(
       QStringLiteral(
           "cancelButton"
       )
   );

   m_cancelButton->setEnabled(false);


   m_startButton =
       new QPushButton(
           QStringLiteral(
               "⚡  开始充电"
           ),
           this
       );

   m_startButton->setObjectName(
       QStringLiteral(
           "startButton"
       )
   );

   m_startButton->setEnabled(false);


   m_finishButton =
       new QPushButton(
           QStringLiteral(
               "结束充电"
           ),
           this
       );

   m_finishButton->setObjectName(
       QStringLiteral(
           "dangerButton"
       )
   );

   m_finishButton->setEnabled(false);
   m_finishButton->hide();


   m_doneButton =
       new QPushButton(
           QStringLiteral(
               "完成"
           ),
           this
       );

   m_doneButton->setObjectName(
       QStringLiteral(
           "doneButton"
       )
   );

   m_doneButton->hide();


   outerLayout->addWidget(
       m_refreshButton
   );

   outerLayout->addWidget(
       m_cancelButton
   );

   outerLayout->addWidget(
       m_startButton
   );

   outerLayout->addWidget(
       m_finishButton
   );

   outerLayout->addWidget(
       m_doneButton
   );


   // ==============================
   // Existing logic
   // ==============================

   connect(
       m_refreshButton,
       &QPushButton::clicked,
       this,
       &OrderSettlementDialog::reloadOrder
   );

   connect(
       m_cancelButton,
       &QPushButton::clicked,
       this,
       &OrderSettlementDialog::onCancelReservation
   );

   connect(
       m_startButton,
       &QPushButton::clicked,
       this,
       &OrderSettlementDialog::onStartCharging
   );

   connect(
       m_finishButton,
       &QPushButton::clicked,
       this,
       &OrderSettlementDialog::onFinishCharging
   );

   connect(
       m_doneButton,
       &QPushButton::clicked,
       this,
       &QDialog::accept
   );
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

    if (info.status == 1
        && info.simulatedSeconds >= kFullChargeSimulatedSeconds
        && !m_autoFinishAttempted) {

        m_autoFinishAttempted = true;

        core::SettlementResult result;
        QString finishError;

        const bool success =
            chargeService.finishCharging(
                m_userId,
                m_orderId,
                result,
                finishError
            );

        if (!success) {
            m_notice->setText(
                QStringLiteral(
                    "充电已达到100%，自动结算失败：%1\n"
                    "请点击“结束充电”重新结算。")
                    .arg(finishError)
            );

            return;
        }

        // 读取完整的小票
        core::OrderReceipt receipt;

        QString receiptError;

        if (!chargeService.getReceipt(
                m_userId,
                m_orderId,
                receipt,
                receiptError)) {

            // 如果读取小票失败，
            // 至少用结算结果显示基本信息。
            receipt.orderId = result.orderId;
            receipt.status = 2;
            receipt.endTime = result.endTime;
            receipt.simulatedSeconds =
                result.simulatedSeconds;
            receipt.energy = result.energy;
            receipt.amount = result.amount;
            receipt.paidAmount =
                result.paidAmount;
            receipt.debtAmount =
                result.debtAmount;
            receipt.balanceAfter =
                result.balanceAfter;
        }

        showReceipt(receipt);
        return;
    }

    const QString startText =
        info.startTime.isEmpty()
            ? QStringLiteral("尚未开始")
            : info.startTime;


    QString html =
        QStringLiteral(
            "<div style=\""
            "color:#FFFFFF;"
            "font-size:16px;"
            "font-weight:700;"
            "margin-bottom:8px;"
            "\">"
            "订单详情"
            "</div>"

            "<table width=\"100%\" "
            "cellspacing=\"0\" "
            "cellpadding=\"0\">"
        );


    html += orderRowHtml(
        QStringLiteral("订单编号"),
        QString::number(info.id)
    );


    html += orderRowHtml(
        QStringLiteral("电站"),
        info.stationName
    );


    html += orderRowHtml(
        QStringLiteral("电桩编号"),
        info.chargerNo
    );


    html += orderRowHtml(
        QStringLiteral("订单状态"),
        statusTextOf(info.status),
        statusColorOf(info.status)
    );


    html += orderRowHtml(
        QStringLiteral("开始时间"),
        startText
    );


    if (info.status == 0) {
        html += orderRowHtml(
            QStringLiteral("预约保留至"),
            info.reservationExpiresAt.isEmpty()
                ? QStringLiteral("—")
                : info.reservationExpiresAt
        );
    }


    if (info.status == 1) {

        html += orderRowHtml(
            QStringLiteral("模拟充电时长"),
            formatDuration(
                info.simulatedSeconds
            )
        );

        html += orderRowHtml(
            QStringLiteral("模拟功率"),
            QStringLiteral("%1 kW")
                .arg(
                    info.power,
                    0,
                    'f',
                    1
                )
        );

        html += orderRowHtml(
            QStringLiteral("单价"),
            QStringLiteral("%1 元/度")
                .arg(
                    info.price,
                    0,
                    'f',
                    2
                )
        );
    }


    html += orderRowHtml(
        QStringLiteral("累计电量"),
        QStringLiteral("%1 度")
            .arg(
                info.energy,
                0,
                'f',
                2
            )
    );


    html += orderRowHtml(
        info.status == 1
            ? QStringLiteral("当前费用")
            : QStringLiteral("订单金额"),

        QStringLiteral("%1 元")
            .arg(
                info.amount,
                0,
                'f',
                2
            ),

        QStringLiteral("#60A5FA")
    );


    html += QStringLiteral(
        "</table>"
    );


    m_details->setText(html);

    // 充电中显示模拟 SoC 进度条（按 1 模拟小时充满估算）。
    const bool isCharging = (info.status == 1);
    m_socLabel->setVisible(isCharging);
    m_socBar->setVisible(isCharging);

    if (isCharging) {
        const int percent = qBound(
            0,
            static_cast<int>(
                info.simulatedSeconds * 100
                / kFullChargeSimulatedSeconds
            ),
            100
        );
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
            "电量达到100%后将自动停止并结算。"));
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

    QString html =
        QStringLiteral(
            "<div style=\""
            "color:#FFFFFF;"
            "font-size:16px;"
            "font-weight:700;"
            "margin-bottom:8px;"
            "\">"
            "充电结算"
            "</div>"

            "<table width=\"100%\" "
            "cellspacing=\"0\" "
            "cellpadding=\"0\">"
        );


    html += orderRowHtml(
        QStringLiteral("订单编号"),
        QString::number(
            receipt.orderId
        )
    );


    html += orderRowHtml(
        QStringLiteral("电站"),
        receipt.stationName.isEmpty()
            ? QStringLiteral("—")
            : receipt.stationName
    );


    html += orderRowHtml(
        QStringLiteral("电桩编号"),
        receipt.chargerNo.isEmpty()
            ? QStringLiteral("—")
            : receipt.chargerNo
    );


    html += orderRowHtml(
        QStringLiteral("开始时间"),
        receipt.startTime.isEmpty()
            ? QStringLiteral("—")
            : receipt.startTime
    );


    html += orderRowHtml(
        QStringLiteral("结束时间"),
        receipt.endTime.isEmpty()
            ? QStringLiteral("—")
            : receipt.endTime
    );


    html += orderRowHtml(
        QStringLiteral("充电时长"),
        formatDuration(
            receipt.simulatedSeconds
        )
    );


    html += orderRowHtml(
        QStringLiteral("充电电量"),
        QStringLiteral("%1 度")
            .arg(
                receipt.energy,
                0,
                'f',
                2
            )
    );


    html += orderRowHtml(
        QStringLiteral("充电单价"),
        QStringLiteral("%1 元/度")
            .arg(
                receipt.unitPrice,
                0,
                'f',
                2
            )
    );


    html += orderRowHtml(
        QStringLiteral("订单金额"),
        QStringLiteral("%1 元")
            .arg(
                receipt.amount,
                0,
                'f',
                2
            ),

        QStringLiteral("#60A5FA")
    );


    if (receipt.paidAmount >= 0) {

        html += orderRowHtml(
            QStringLiteral("实际扣款"),
            QStringLiteral("%1 元")
                .arg(
                    receipt.paidAmount,
                    0,
                    'f',
                    2
                ),

            QStringLiteral("#34D399")
        );


        html += orderRowHtml(
            QStringLiteral("扣款后余额"),
            QStringLiteral("%1 元")
                .arg(
                    receipt.balanceAfter,
                    0,
                    'f',
                    2
                )
        );


        if (receipt.debtAmount > 0) {

            html += orderRowHtml(
                QStringLiteral("本次欠费"),
                QStringLiteral("%1 元")
                    .arg(
                        receipt.debtAmount,
                        0,
                        'f',
                        2
                    ),

                QStringLiteral("#FB7185")
            );
        }

    } else {

        html += orderRowHtml(
            QStringLiteral("扣款后余额"),
            QStringLiteral("结算时确定")
        );
    }


    html += QStringLiteral(
        "</table>"
    );


    m_details->setText(html);

    m_details->setAlignment(
        Qt::AlignLeft |
        Qt::AlignTop
    );
    m_details->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    m_socLabel->hide();
    m_socBar->hide();
    m_notice->setText(QStringLiteral(
        "充电已完成，电桩已恢复空闲，费用已从余额扣除。"));

    setButtonsVisible(false, false, true);
}
