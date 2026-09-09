#pragma once
// ============================================================
// 用户端主窗口（UC-U-02）
// 手机竖屏比例 420×760（NFR-U-02），默认进入电站列表页。
// ============================================================
#include <QMainWindow>

#include "service/stationservice.h"
#include "service/userservice.h"

class QDialog;
class QLabel;
class QPushButton;
class QStackedWidget;

namespace client_user { class StationListPage; }

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(core::StationService *service,
                        const QString &initialRegion = QString(),
                        QWidget *parent = nullptr);

    void setCurrentUser(const UserInfo &user);

signals:
    void personalCenterRequested();
    void chargingRequested(int stationId, int chargerId);
    void settlementRequested(int orderId);

private slots:
    void onStationClicked(const core::StationListItem &item);

private:
    void setupUi();
    void setupStyle();
    QDialog *buildStationDetailDialog(const core::StationListItem &item);
    UserInfo m_currentUser;

    // 返回 true 才允许进入选桩流程。
    bool checkChargingEntry(QWidget *messageParent);

    core::StationService *m_service = nullptr;
    client_user::StationListPage *m_stationPage = nullptr;
    QStackedWidget *m_stack = nullptr;
    QPushButton *m_navStationBtn = nullptr;
    QPushButton *m_navMineBtn = nullptr;
};
