#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QModelIndex>
#include <QProgressDialog>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTableView>
#include <QTableWidget>
#include <QVector>

#include <QtCharts/QChartView>
#include <QList>

#include "service/chargerservice.h"

#include "service/adminauthservice.h"
#include "service/statsservice.h"
#include "service/stationservice.h"
#include "service/adminuserservice.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QChart;
class QLineSeries;
class QBarSeries;
class QLabel;
class QTableWidget;
class QComboBox;
class QLineEdit;
class QTableView;
class QStandardItemModel;
class QPushButton;
class QProgressDialog;
class QSortFilterProxyModel;
class QSpinBox;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void setCurrentAdmin(const AdminAuthService::AdminInfo &admin);

signals:
    void logoutRequested();

private slots:
    void refreshDashboard();
    void logout();
    void selectPage(int index);
    void selectRevenuePeriod(int days);

private:
    void setupRevenueCharts();
    void setupChargerStatusOverview();
    void rebuildRevenueCharts(const StatsService::RevenueSummary &summary);
    void updateRevenueMetrics(const StatsService::RevenueSummary &summary);
    void updateClock();
    void updateOnlineChargerCount();
    void updateDatabaseStatus();
    void showRevenueEmptyState(bool empty);
    void refreshChargerStatusOverview();
    void rebuildChargerStatusChart(const StatsService::ChargerStatusSummary &summary);
    void setupChargerManagement();
    void setupStationManagement();
    void refreshChargerManagement();
    void applyChargerFilters();
    int selectedChargerId() const;
    int selectedChargerStatus() const;
    void restartSelectedCharger();
    void setSelectedChargerFault();
    void recoverSelectedCharger();
    void addNewCharger();
    void deleteSelectedCharger();
    void refreshStationManagement(int preferredStationId = -1);
    void stationSelectionChanged();
    void editSelectedStation();
    void addNewStation();
    void deleteSelectedStation();
    void setupUserManagement();
    void refreshUserManagement();
    void userSelectionChanged(const QModelIndex &current, const QModelIndex &previous);
    void toggleSelectedUserStatus();
    void showSelectedUserOrders();

    Ui::MainWindow *ui;
    AdminAuthService::AdminInfo m_currentAdmin;
    StatsService m_statsService;
    int m_revenueDays = 30;

    QChartView *m_revenueChartView = nullptr;
    QChartView *m_orderChartView = nullptr;
    QLabel *m_revenueEmptyLabel = nullptr;
    QLabel *m_orderEmptyLabel = nullptr;

    QChartView *m_statusChartView = nullptr;
    QTableWidget *m_statusTable = nullptr;
    QLabel *m_healthScoreLabel = nullptr;
    QLabel *m_statusQueryPerformanceLabel = nullptr;
    QLabel *m_statusEmptyLabel = nullptr;

    ChargerService m_chargerService;
    QList<ChargerService::ChargerRecord> m_chargerRecords;
    QComboBox *m_stationFilter = nullptr;
    QComboBox *m_statusFilter = nullptr;
    QLineEdit *m_chargerSearch = nullptr;
    QTableView *m_chargerTable = nullptr;
    QStandardItemModel *m_chargerModel = nullptr;
    QPushButton *m_restartChargerButton = nullptr;
    QPushButton *m_faultChargerButton = nullptr;
    QPushButton *m_recoverChargerButton = nullptr;
    QPushButton *m_addChargerButton = nullptr;
    QPushButton *m_deleteChargerButton = nullptr;
    QProgressDialog *m_restartProgress = nullptr;
    int m_restartChargerId = 0;

    StationService m_stationService;
    QList<StationService::StationRecord> m_stationRecords;
    QTableWidget *m_stationTable = nullptr;
    QTableWidget *m_stationChargerTable = nullptr;
    QLabel *m_stationSummaryLabel = nullptr;
    QPushButton *m_addStationButton = nullptr;
    QPushButton *m_editStationButton = nullptr;
    QPushButton *m_deleteStationButton = nullptr;
    int m_selectedStationId = 0;

    AdminUserService m_adminUserService;
    QList<AdminUserService::UserRecord> m_userRecords;
    QTableView *m_userTable = nullptr;
    QStandardItemModel *m_userModel = nullptr;
    QLineEdit *m_userSearch = nullptr;
    QPushButton *m_userStatusButton = nullptr;
    QLabel *m_userSummaryLabel = nullptr;
    int m_selectedUserId = 0;
};

#endif // MAINWINDOW_H
