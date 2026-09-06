#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "mainwindowhelpers.h"

#include <algorithm>
#include <QAbstractItemView>
#include <QBrush>
#include <QColor>
#include <QDate>
#include <QDateTime>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSqlDatabase>
#include <QTableView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QStandardItemModel>
#include <QStandardItem>

#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QChart>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setMinimumSize(1280, 800);

    connect(uiObject<QPushButton>(this, QStringLiteral("refreshButton")), &QPushButton::clicked,
            this, &MainWindow::refreshDashboard);
    connect(uiObject<QPushButton>(this, QStringLiteral("logoutButton")), &QPushButton::clicked,
            this, &MainWindow::logout);

    connect(uiObject<QPushButton>(this, QStringLiteral("navRevenueButton")), &QPushButton::clicked,
            this, [this] { selectPage(0); });
    connect(uiObject<QPushButton>(this, QStringLiteral("navChargerStatusButton")), &QPushButton::clicked,
            this, [this] { selectPage(1); });
    connect(uiObject<QPushButton>(this, QStringLiteral("navChargerManageButton")), &QPushButton::clicked,
            this, [this] { selectPage(2); });
    connect(uiObject<QPushButton>(this, QStringLiteral("navStationManageButton")), &QPushButton::clicked,
            this, [this] { selectPage(3); });
    connect(uiObject<QPushButton>(this, QStringLiteral("navUserManageButton")), &QPushButton::clicked,
            this, [this] { selectPage(4); });
    connect(uiObject<QPushButton>(this, QStringLiteral("navPredictionButton")), &QPushButton::clicked,
            this, [this] { selectPage(5); });

    connect(uiObject<QPushButton>(this, QStringLiteral("period7Button")), &QPushButton::clicked,
            this, [this] { selectRevenuePeriod(7); });
    connect(uiObject<QPushButton>(this, QStringLiteral("period30Button")), &QPushButton::clicked,
            this, [this] { selectRevenuePeriod(30); });

    auto *timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, [this] {
        updateClock();
    });

    setupRevenueCharts();
    setupChargerStatusOverview();
    setupChargerManagement();
    setupStationManagement();
    setupUserManagement();
    selectPage(0);
    timer->start();
    refreshDashboard();
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::setCurrentAdmin(
    const AdminAuthService::AdminInfo &admin)
{
    m_currentAdmin = admin;

    uiObject<QLabel>(this, QStringLiteral("currentAdminLabel"))->setText(
        QStringLiteral("管理员：%1").arg(admin.username));
    uiObject<QLabel>(this, QStringLiteral("adminStatusLabel"))->setText(
        QStringLiteral("管理员：%1").arg(admin.username));

    setWindowTitle(
        QStringLiteral(
            "东软电动汽车充电桩应用管理平台 · 管理后台"));
}


void MainWindow::selectPage(int index)
{
    uiObject<QStackedWidget>(this, QStringLiteral("stackedWidget"))->setCurrentIndex(index);

    const QList<QPushButton *> buttons = {
        uiObject<QPushButton>(this, QStringLiteral("navRevenueButton")),
        uiObject<QPushButton>(this, QStringLiteral("navChargerStatusButton")),
        uiObject<QPushButton>(this, QStringLiteral("navChargerManageButton")),
        uiObject<QPushButton>(this, QStringLiteral("navStationManageButton")),
        uiObject<QPushButton>(this, QStringLiteral("navUserManageButton")),
        uiObject<QPushButton>(this, QStringLiteral("navPredictionButton"))
    };

    for (int i = 0; i < buttons.size(); ++i) {
        buttons.at(i)->setChecked(i == index);
    }

    const QStringList titles = {
        QStringLiteral("营收分析"),
        QStringLiteral("电桩状态"),
        QStringLiteral("充电桩管理"),
        QStringLiteral("充电站管理"),
        QStringLiteral("用户管理"),
        QStringLiteral("智能预测")
    };

    if (index >= 0 && index < titles.size()) {
        uiObject<QLabel>(this, QStringLiteral("pageTitleLabel"))->setText(titles.at(index));
    }

    // Revenue is the only page currently backed by UC-A-03 data.
    if (index == 0) {
        refreshDashboard();
    } else if (index == 1) {
        refreshChargerStatusOverview();
    } else if (index == 2) {
        refreshChargerManagement();
    } else if (index == 3) {
        refreshStationManagement(m_selectedStationId);
    } else if (index == 4) {
        refreshUserManagement();
    }
}


void MainWindow::refreshDashboard()
{
    updateClock();
    updateOnlineChargerCount();
    updateDatabaseStatus();

    StatsService::RevenueSummary summary;
    QString errorMessage;

    if (!m_statsService.loadRevenueSummary(
            m_revenueDays, summary, errorMessage)) {
        uiObject<QLabel>(this, QStringLiteral("metricValue1"))->setText(QStringLiteral("¥ 0.00"));
        uiObject<QLabel>(this, QStringLiteral("metricValue2"))->setText(QStringLiteral("¥ 0.00"));
        uiObject<QLabel>(this, QStringLiteral("metricValue3"))->setText(QStringLiteral("¥ 0.00"));
        uiObject<QLabel>(this, QStringLiteral("metricAccent1"))->setText(QStringLiteral("数据读取失败"));
        uiObject<QLabel>(this, QStringLiteral("metricAccent2"))->setText(QStringLiteral("数据读取失败"));
        uiObject<QLabel>(this, QStringLiteral("metricAccent3"))->setText(QStringLiteral("数据读取失败"));

        m_revenueChartView->setChart(new QChart());
        m_orderChartView->setChart(new QChart());
        showRevenueEmptyState(true);

        uiObject<QLabel>(this, QStringLiteral("lastRefreshLabel"))->setText(
            QStringLiteral("刷新失败：%1").arg(errorMessage));
        return;
    }

    updateRevenueMetrics(summary);
    rebuildRevenueCharts(summary);

    uiObject<QLabel>(this, QStringLiteral("chartTitle"))->setText(
        QStringLiteral("近 %1 日营收趋势").arg(m_revenueDays));
    uiObject<QLabel>(this, QStringLiteral("orderChartTitle"))->setText(
        QStringLiteral("近 %1 日每日订单量").arg(m_revenueDays));

    uiObject<QLabel>(this, QStringLiteral("queryPerformanceLabel"))->setText(
        QStringLiteral("30天聚合查询：%1 ms · 目标 < 300 ms")
            .arg(summary.elapsedMs));

    uiObject<QLabel>(this, QStringLiteral("lastRefreshLabel"))->setText(
        QStringLiteral("最后刷新：%1")
            .arg(QDateTime::currentDateTime()
                     .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));

    const int pageIndex = uiObject<QStackedWidget>(this, QStringLiteral("stackedWidget"))->currentIndex();
    if (pageIndex == 1) {
        refreshChargerStatusOverview();
    } else if (pageIndex == 2) {
        refreshChargerManagement();
    } else if (pageIndex == 3) {
        refreshStationManagement(m_selectedStationId);
    }
}


void MainWindow::updateClock()
{
    uiObject<QLabel>(this, QStringLiteral("clockLabel"))->setText(
        QDateTime::currentDateTime()
            .toString(QStringLiteral("yyyy-MM-dd  HH:mm:ss")));
}


void MainWindow::updateOnlineChargerCount()
{
    int count = 0;
    QString errorMessage;
    if (!m_statsService.onlineChargerCount(count, errorMessage)) {
        uiObject<QLabel>(this, QStringLiteral("onlineChargerLabel"))->setText(QStringLiteral("在线电桩：--"));
        return;
    }

    uiObject<QLabel>(this, QStringLiteral("onlineChargerLabel"))->setText(
        QStringLiteral("在线电桩：%1").arg(count));
}


void MainWindow::updateDatabaseStatus()
{
    const QSqlDatabase db =
        QSqlDatabase::database(QStringLiteral("ncs_connection"));

    if (db.isValid() && db.isOpen()) {
        uiObject<QLabel>(this, QStringLiteral("databaseStatusLabel"))->setText(
            QStringLiteral("数据库：SQLite · 已连接"));
        uiObject<QLabel>(this, QStringLiteral("databasePathLabel"))->setText(
            QStringLiteral("数据库：%1").arg(db.databaseName()));
    } else {
        uiObject<QLabel>(this, QStringLiteral("databaseStatusLabel"))->setText(
            QStringLiteral("数据库：未连接"));
        uiObject<QLabel>(this, QStringLiteral("databasePathLabel"))->setText(QStringLiteral("数据库：--"));
    }
}


void MainWindow::logout()
{
    const auto result = QMessageBox::question(
        this,
        QStringLiteral("退出登录"),
        QStringLiteral("确定退出当前管理员账号吗？"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (result != QMessageBox::Yes) {
        return;
    }

    m_currentAdmin = AdminAuthService::AdminInfo{};
    hide();
    emit logoutRequested();
}
