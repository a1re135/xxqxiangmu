#pragma once
// ============================================================
// 电站列表页（UC-U-02 附近充电站查询）
// 页面顶部：区域下拉框 + 地址输入框 + 定位按钮；
// 列表区：电站卡片（站名/单价/空闲数/距离），按距离升序；
// 空表显示占位文案「暂无充电站数据」。
// ============================================================
#include <QWidget>

#include "../../core/service/stationservice.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QTimer;
class QVBoxLayout;

namespace client_user {

class StationListPage : public QWidget {
    Q_OBJECT
public:
    explicit StationListPage(core::StationService *service,
                             QWidget *parent = nullptr);

    // 设置初始定位区域（在首次显示前调用）
    void setInitialRegion(const QString &regionName);

    // 按最近一次定位位置重新查询并刷新列表（供刷新按钮/外部调用）
    void refresh();

signals:
    void backToHomeRequested();
    void stationClicked(const core::StationListItem &item);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onLocateClicked();
    void onLocated(const core::LocationResult &result);

private:
    void rebuildList(const QVector<core::StationListItem> &items);
    void showToast(const QString &text, bool isError);
    QVector<core::StationListItem> currentList() const;
    void clearCards();

    core::StationService *m_service = nullptr;

    QComboBox *m_regionCombo = nullptr;
    QLineEdit *m_addressEdit = nullptr;
    QPushButton *m_locateBtn = nullptr;
    QLabel *m_locationLabel = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QLabel *m_toastLabel = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QWidget *m_cardsContainer = nullptr;
    QVBoxLayout *m_outerLayout = nullptr; // 卡片区 + 空态标签的外层布局
    QVBoxLayout *m_cardsLayout = nullptr;
    QLabel *m_emptyLabel = nullptr;

    QTimer *m_toastTimer = nullptr;
    bool m_firstShow = true;
    bool m_locating = false;
};

} // namespace client_user
