#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "mainwindowhelpers.h"

#include <algorithm>
#include <QDate>
#include <QDateTime>
#include <QFont>
#include <QInputDialog>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QSignalBlocker>
#include <QSqlDatabase>
#include <QTime>
#include <QFormLayout>
#include <QGroupBox>

#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QChart>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QAbstractItemView>
#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLayout>
#include <QLayoutItem>
#include <QLineEdit>
#include <QMessageBox>
#include <QModelIndex>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

void MainWindow::setupRevenueCharts()
{
    auto *revenueLayout = new QGridLayout(uiObject<QWidget>(this, QStringLiteral("revenueChartHost")));
    revenueLayout->setContentsMargins(0, 0, 0, 0);
    revenueLayout->setSpacing(0);

    m_revenueChartView = new QChartView(uiObject<QWidget>(this, QStringLiteral("revenueChartHost")));
    m_revenueChartView->setRenderHint(QPainter::Antialiasing);
    m_revenueChartView->setFrameShape(QFrame::NoFrame);
    m_revenueChartView->setStyleSheet(QStringLiteral("background: transparent; border: none;"));

    m_revenueEmptyLabel = new QLabel(uiObject<QWidget>(this, QStringLiteral("revenueChartHost")));
    m_revenueEmptyLabel->setAlignment(Qt::AlignCenter);
    m_revenueEmptyLabel->setText(QStringLiteral("暂无数据"));
    m_revenueEmptyLabel->setStyleSheet(QStringLiteral(
        "QLabel { color:#8EA0AB; font-size:18px; font-weight:800; "
        "background:rgba(18,29,48,190); padding:10px 18px; border-radius:10px; }"));
    m_revenueEmptyLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    revenueLayout->addWidget(m_revenueChartView, 0, 0);
    revenueLayout->addWidget(m_revenueEmptyLabel, 0, 0, Qt::AlignCenter);

    auto *orderLayout = new QGridLayout(uiObject<QWidget>(this, QStringLiteral("orderChartHost")));
    orderLayout->setContentsMargins(0, 0, 0, 0);
    orderLayout->setSpacing(0);

    m_orderChartView = new QChartView(uiObject<QWidget>(this, QStringLiteral("orderChartHost")));
    m_orderChartView->setRenderHint(QPainter::Antialiasing);
    m_orderChartView->setFrameShape(QFrame::NoFrame);
    m_orderChartView->setStyleSheet(QStringLiteral("background: transparent; border: none;"));

    m_orderEmptyLabel = new QLabel(uiObject<QWidget>(this, QStringLiteral("orderChartHost")));
    m_orderEmptyLabel->setAlignment(Qt::AlignCenter);
    m_orderEmptyLabel->setText(QStringLiteral("暂无数据"));
    m_orderEmptyLabel->setStyleSheet(QStringLiteral(
        "QLabel { color:#8EA0AB; font-size:18px; font-weight:800; "
        "background:rgba(18,29,48,190); padding:10px 18px; border-radius:10px; }"));
    m_orderEmptyLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    orderLayout->addWidget(m_orderChartView, 0, 0);
    orderLayout->addWidget(m_orderEmptyLabel, 0, 0, Qt::AlignCenter);

    m_revenueEmptyLabel->raise();
    m_orderEmptyLabel->raise();
}


void MainWindow::rebuildRevenueCharts(
    const StatsService::RevenueSummary &summary)
{
    // Revenue line chart.
    auto *revenueChart = new QChart();
    styleChart(revenueChart);
    revenueChart->setTitle(QString());

    auto *lineSeries = new QLineSeries(revenueChart);
    lineSeries->setName(QStringLiteral("营收"));
    QPen linePen(QColor(QStringLiteral("#10B981")));
    linePen.setWidth(3);
    lineSeries->setPen(linePen);
    lineSeries->setPointsVisible(true);
    lineSeries->setPointLabelsVisible(false);

    bool anyRevenue = false;
    double maxRevenue = 0.0;
    for (const auto &point : summary.points) {
        const qreal x = QDateTime(point.date,
                                  QTime(0, 0, 0))
                            .toMSecsSinceEpoch();
        lineSeries->append(x, point.revenue);
        maxRevenue = qMax(maxRevenue, point.revenue);
        anyRevenue = anyRevenue || point.revenue > 0.0;
    }
    revenueChart->addSeries(lineSeries);

    auto *xAxis = new QDateTimeAxis(revenueChart);
    xAxis->setFormat(QStringLiteral("MM-dd"));
    xAxis->setLabelsFont(chartAxisFont(9));
    xAxis->setLabelsColor(QColor(QStringLiteral("#80909B")));
    xAxis->setLinePenColor(QColor(QStringLiteral("#304353")));
    xAxis->setGridLineVisible(false);

    const QDate startDate = summary.points.isEmpty()
        ? QDate::currentDate()
        : summary.points.first().date;
    const QDate endDate = summary.points.isEmpty()
        ? QDate::currentDate()
        : summary.points.last().date;
    xAxis->setRange(
        QDateTime(startDate, QTime(0, 0, 0)),
        QDateTime(endDate, QTime(0, 0, 0)));

    auto *yAxis = new QValueAxis(revenueChart);
    yAxis->setTitleText(QStringLiteral("营收（元）"));
    yAxis->setLabelsFont(chartAxisFont(9));
    yAxis->setLabelsColor(QColor(QStringLiteral("#80909B")));
    yAxis->setTitleBrush(QBrush(QColor(QStringLiteral("#8FA1AC"))));
    yAxis->setGridLineColor(QColor(QStringLiteral("#253747")));
    yAxis->setLinePenColor(QColor(QStringLiteral("#304353")));
    yAxis->setMin(0.0);
    yAxis->setMax(anyRevenue ? qMax(1.0, maxRevenue * 1.20) : 1.0);
    yAxis->setTickCount(5);

    revenueChart->addAxis(xAxis, Qt::AlignBottom);
    revenueChart->addAxis(yAxis, Qt::AlignLeft);
    lineSeries->attachAxis(xAxis);
    lineSeries->attachAxis(yAxis);

    m_revenueChartView->setChart(revenueChart);

    // Completed orders bar chart.
    auto *orderChart = new QChart();
    styleChart(orderChart);

    auto *barSeries = new QBarSeries(orderChart);
    auto *barSet = new QBarSet(QStringLiteral("订单量"));
    barSet->setColor(QColor(QStringLiteral("#3DBA91")));

    QStringList categories;
    int maxOrders = 0;
    bool anyOrders = false;

    for (const auto &point : summary.points) {
        categories << point.date.toString(QStringLiteral("MM-dd"));
        *barSet << point.orderCount;
        maxOrders = qMax(maxOrders, point.orderCount);
        anyOrders = anyOrders || point.orderCount > 0;
    }

    barSeries->append(barSet);
    barSeries->setBarWidth(0.62);
    orderChart->addSeries(barSeries);

    auto *barXAxis = new QBarCategoryAxis(orderChart);
    barXAxis->append(categories);
    barXAxis->setLabelsFont(chartAxisFont(8));
    barXAxis->setLabelsColor(QColor(QStringLiteral("#80909B")));
    barXAxis->setLinePenColor(QColor(QStringLiteral("#304353")));
    barXAxis->setGridLineVisible(false);
    barXAxis->setLabelsAngle(categories.size() > 14 ? -45 : 0);

    auto *barYAxis = new QValueAxis(orderChart);
    barYAxis->setTitleText(QStringLiteral("订单量（笔）"));
    barYAxis->setLabelsFont(chartAxisFont(9));
    barYAxis->setLabelsColor(QColor(QStringLiteral("#80909B")));
    barYAxis->setTitleBrush(QBrush(QColor(QStringLiteral("#8FA1AC"))));
    barYAxis->setGridLineColor(QColor(QStringLiteral("#253747")));
    barYAxis->setLinePenColor(QColor(QStringLiteral("#304353")));
    barYAxis->setMin(0);
    barYAxis->setMax(anyOrders ? qMax(1, maxOrders + 1) : 1);
    barYAxis->setTickCount(qMin(6, qMax(2, maxOrders + 2)));

    orderChart->addAxis(barXAxis, Qt::AlignBottom);
    orderChart->addAxis(barYAxis, Qt::AlignLeft);
    barSeries->attachAxis(barXAxis);
    barSeries->attachAxis(barYAxis);

    m_orderChartView->setChart(orderChart);

    const bool hasAnyData = anyRevenue || anyOrders;
    showRevenueEmptyState(!hasAnyData);
}


void MainWindow::updateRevenueMetrics(
    const StatsService::RevenueSummary &summary)
{
    uiObject<QLabel>(this, QStringLiteral("metricValue1"))->setText(
        QStringLiteral("¥ %1").arg(summary.todayRevenue, 0, 'f', 2));
    uiObject<QLabel>(this, QStringLiteral("metricValue2"))->setText(
        QStringLiteral("¥ %1").arg(summary.monthRevenue, 0, 'f', 2));
    uiObject<QLabel>(this, QStringLiteral("metricValue3"))->setText(
        QStringLiteral("¥ %1").arg(summary.totalRevenue, 0, 'f', 2));

    uiObject<QLabel>(this, QStringLiteral("metricAccent1"))->setText(
        QStringLiteral("今日完成订单 %1 笔").arg(summary.todayOrders));
    uiObject<QLabel>(this, QStringLiteral("metricAccent2"))->setText(
        QStringLiteral("本月完成订单 %1 笔").arg(summary.monthOrders));
    uiObject<QLabel>(this, QStringLiteral("metricAccent3"))->setText(
        QStringLiteral("历史完成订单 %1 笔").arg(summary.totalOrders));
}


void MainWindow::showRevenueEmptyState(bool empty)
{
    m_revenueEmptyLabel->setVisible(empty);
    m_orderEmptyLabel->setVisible(empty);
}


void MainWindow::selectRevenuePeriod(int days)
{
    m_revenueDays = (days == 7) ? 7 : 30;
    uiObject<QPushButton>(this, QStringLiteral("period7Button"))->setChecked(m_revenueDays == 7);
    uiObject<QPushButton>(this, QStringLiteral("period30Button"))->setChecked(m_revenueDays == 30);
    refreshDashboard();
}

