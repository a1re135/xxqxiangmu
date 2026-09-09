#pragma once
// ============================================================
// 我的订单（UC-U-10）
// 当前用户历史订单列表，倒序展示电站名/电桩编号/时间/
// 电量/金额/状态；双击或点击“查看小票”查看订单小票详情。
// ============================================================
#include <QDialog>

#include <QVector>

class QLabel;
class QPushButton;
class QTableWidget;

class OrderHistoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OrderHistoryDialog(
        int userId,
        QWidget *parent = nullptr);

private slots:
    void loadOrders();
    void showSelectedReceipt();
    void onRowDoubleClicked(int row, int column);

private:
    void buildUi();
    void showReceiptOf(int orderId);

    int m_userId = -1;

    QVector<int> m_orderIds;

    QTableWidget *m_table = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QPushButton *m_receiptButton = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_closeButton = nullptr;
};
