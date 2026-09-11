#pragma once
// ============================================================
// 订单结算/充电控制弹窗（UC-U-07/08/09）
// 预约中：可开始充电、取消预约，超时自动取消（E3）；
// 充电中：每秒刷新实时计费 + 模拟 SoC 进度条；
// 已结算：展示完整订单小票（订单号/电站/电桩/起止时间/
// 充电时长/电量/单价/总金额/扣款后余额），提供“完成”返回。
// ============================================================
#include <QDialog>

#include "service/charge_service.h"

class QLabel;
class QProgressBar;
class QPushButton;
class QScrollArea;
class QTimer;
class QWidget;

class OrderSettlementDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OrderSettlementDialog(
        int orderId,
        int userId,
        QWidget *parent = nullptr);

private slots:
    void reloadOrder();

private:
    void buildUi();
    void onFinishCharging();
    void onCancelReservation();
    void onStartCharging();
    void showReceipt(const core::OrderReceipt &receipt);
    bool tryAutoCancelExpired(
        const core::ChargeOrderInfo &info,
        QString &message);
    void setButtonsVisible(
        bool reserved,
        bool charging,
        bool settled);

    int m_orderId = -1;
    int m_userId = -1;
    bool m_expiredHandled = false;
    bool m_settledShown = false;
    bool m_autoFinishAttempted = false;

    QLabel *m_title = nullptr;
    QLabel *m_details = nullptr;
    QLabel *m_liveDetails = nullptr;
    QLabel *m_socLabel = nullptr;
    QProgressBar *m_socBar = nullptr;
    QLabel *m_notice = nullptr;
    QScrollArea *m_scroll = nullptr;
    QWidget *m_content = nullptr;

    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_finishButton = nullptr;
    QPushButton *m_doneButton = nullptr;

    QTimer *m_refreshTimer = nullptr;
};
