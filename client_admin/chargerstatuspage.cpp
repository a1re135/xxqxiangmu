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

void MainWindow::setupChargerStatusOverview()
{
    auto *page = uiObject<QWidget>(this, QStringLiteral("chargerStatusPage"));
    if (!page) {
        return;
    }

    // Replace the UC-A-02 placeholder widgets with the real UC-A-04 content.
    if (auto *placeholderTitle = uiObject<QLabel>(page, QStringLiteral("pagePlaceholderTitlechargerStatusPage"))) {
        placeholderTitle->hide();
    }
    if (auto *placeholderText = uiObject<QLabel>(page, QStringLiteral("pagePlaceholderTextchargerStatusPage"))) {
        placeholderText->hide();
    }
    if (auto *placeholderIcon = uiObject<QLabel>(page, QStringLiteral("pageIconchargerStatusPage"))) {
        placeholderIcon->hide();
    }

    // chargerStatusPage already owns this layout from mainwindow.ui.
    // Reusing it prevents the "widget already has a layout" problem.
    auto *rootLayout = uiObject<QVBoxLayout>(this, QStringLiteral("chargerStatusPageLayout"));
    if (!rootLayout) {
        return;
    }

    // Remove the Designer placeholder items (including expanding spacers),
    // otherwise they push the real UC-A-04 content out of the visible area.
    while (rootLayout->count() > 0) {
        QLayoutItem *item = rootLayout->takeAt(0);
        if (QWidget *widget = item->widget()) {
            widget->hide();
        }
        delete item;
    }

    rootLayout->setContentsMargins(28, 24, 28, 24);
    rootLayout->setSpacing(18);

    auto *introLayout = new QHBoxLayout();
    introLayout->setSpacing(14);

    auto *icon = new QLabel(page);
    icon->setFixedSize(46, 46);
    icon->setAlignment(Qt::AlignCenter);
    icon->setText(QStringLiteral("◉"));
    icon->setStyleSheet(QStringLiteral(
        "QLabel { color:#10B981; background:#102D2B; border:1px solid #1B5148; "
        "border-radius:14px; font-size:23px; font-weight:900; }"));

    auto *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(3);
    auto *title = new QLabel(QStringLiteral("电桩状态总览"), page);
    title->setStyleSheet(QStringLiteral(
        "QLabel { color:#F5F7FA; font-size:24px; font-weight:900; }"));
    auto *subtitle = new QLabel(QStringLiteral("实时统计在用、闲置与故障设备，并计算整体设备健康度"), page);
    subtitle->setStyleSheet(QStringLiteral(
        "QLabel { color:#8FA1AC; font-size:14px; font-weight:600; }"));
    titleLayout->addWidget(title);
    titleLayout->addWidget(subtitle);
    introLayout->addWidget(icon);
    introLayout->addLayout(titleLayout);
    introLayout->addStretch();

    m_statusQueryPerformanceLabel = new QLabel(QStringLiteral("统计耗时 -- ms"), page);
    m_statusQueryPerformanceLabel->setAlignment(Qt::AlignCenter);
    m_statusQueryPerformanceLabel->setMinimumWidth(150);
    m_statusQueryPerformanceLabel->setStyleSheet(QStringLiteral(
        "QLabel { color:#8FA1AC; background:#162336; border:1px solid #2B3A4D; "
        "border-radius:10px; padding:9px 12px; font-size:13px; font-weight:700; }"));
    introLayout->addWidget(m_statusQueryPerformanceLabel);
    rootLayout->addLayout(introLayout);

    auto *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(18);

    // Left: status table.
    auto *tableCard = new QFrame(page);
    tableCard->setObjectName(QStringLiteral("chargerStatusTableCard"));
    tableCard->setStyleSheet(QStringLiteral(
        "QFrame#chargerStatusTableCard { background:#121D30; border:1px solid #25364A; border-radius:16px; }"));
    auto *tableLayout = new QVBoxLayout(tableCard);
    tableLayout->setContentsMargins(20, 18, 20, 16);
    tableLayout->setSpacing(12);

    auto *tableTitle = new QLabel(QStringLiteral("状态明细"), tableCard);
    tableTitle->setStyleSheet(QStringLiteral(
        "QLabel { color:#F3F6F8; font-size:19px; font-weight:850; }"));
    auto *tableHint = new QLabel(QStringLiteral("数量与占比基于当前全部电桩实时统计"), tableCard);
    tableHint->setStyleSheet(QStringLiteral(
        "QLabel { color:#718394; font-size:12px; font-weight:600; }"));
    tableLayout->addWidget(tableTitle);
    tableLayout->addWidget(tableHint);

    m_statusTable = new QTableWidget(3, 3, tableCard);
    m_statusTable->setHorizontalHeaderLabels({QStringLiteral("状态"), QStringLiteral("数量"), QStringLiteral("占比")});
    m_statusTable->verticalHeader()->setVisible(false);
    m_statusTable->horizontalHeader()->setStretchLastSection(true);
    m_statusTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_statusTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_statusTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_statusTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_statusTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_statusTable->setFocusPolicy(Qt::NoFocus);
    m_statusTable->setShowGrid(false);
    m_statusTable->setAlternatingRowColors(false);
    m_statusTable->setStyleSheet(QStringLiteral(
        "QTableWidget { background:#101A2B; border:1px solid #27394D; border-radius:11px; "
        "color:#E8EEF2; font-size:15px; font-weight:650; gridline-color:transparent; }"
        "QHeaderView::section { background:#1A2739; color:#8FA1AC; padding:11px 10px; "
        "border:none; font-size:13px; font-weight:800; }"
        "QTableWidget::item { padding:9px 10px; border-bottom:1px solid #1D2B3C; }"));
    m_statusTable->setRowHeight(0, 54);
    m_statusTable->setRowHeight(1, 54);
    m_statusTable->setRowHeight(2, 54);
    tableLayout->addWidget(m_statusTable, 1);
    contentLayout->addWidget(tableCard, 5);

    // Right: pie chart + health score.
    auto *visualCard = new QFrame(page);
    visualCard->setObjectName(QStringLiteral("chargerStatusVisualCard"));
    visualCard->setStyleSheet(QStringLiteral(
        "QFrame#chargerStatusVisualCard { background:#121D30; border:1px solid #25364A; border-radius:16px; }"));
    auto *visualLayout = new QVBoxLayout(visualCard);
    visualLayout->setContentsMargins(20, 18, 20, 18);
    visualLayout->setSpacing(10);

    auto *visualTitle = new QLabel(QStringLiteral("状态占比"), visualCard);
    visualTitle->setStyleSheet(QStringLiteral(
        "QLabel { color:#F3F6F8; font-size:19px; font-weight:850; }"));
    visualLayout->addWidget(visualTitle);

    auto *chartHost = new QWidget(visualCard);
    chartHost->setMinimumHeight(300);
    auto *chartLayout = new QVBoxLayout(chartHost);
    chartLayout->setContentsMargins(0, 0, 0, 0);
    m_statusChartView = new QChartView(chartHost);
    m_statusChartView->setRenderHint(QPainter::Antialiasing);
    m_statusChartView->setFrameShape(QFrame::NoFrame);
    m_statusChartView->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    chartLayout->addWidget(m_statusChartView);
    visualLayout->addWidget(chartHost, 1);

    m_statusEmptyLabel = new QLabel(QStringLiteral("暂无数据"), visualCard);
    m_statusEmptyLabel->setAlignment(Qt::AlignCenter);
    m_statusEmptyLabel->setStyleSheet(QStringLiteral(
        "QLabel { color:#8EA0AB; font-size:18px; font-weight:800; background:#101A2B; "
        "border:1px solid #293A4D; padding:10px 18px; border-radius:10px; }"));
    m_statusEmptyLabel->setVisible(false);
    visualLayout->addWidget(m_statusEmptyLabel, 0, Qt::AlignCenter);

    auto *healthCard = new QFrame(visualCard);
    healthCard->setStyleSheet(QStringLiteral(
        "QFrame { background:#10282B; border:1px solid #205249; border-radius:12px; }"));
    auto *healthLayout = new QHBoxLayout(healthCard);
    healthLayout->setContentsMargins(16, 12, 16, 12);
    auto *healthText = new QLabel(QStringLiteral("设备健康度"), healthCard);
    healthText->setStyleSheet(QStringLiteral(
        "QLabel { color:#9FB4B6; font-size:14px; font-weight:800; }"));
    m_healthScoreLabel = new QLabel(QStringLiteral("0.00%"), healthCard);
    m_healthScoreLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_healthScoreLabel->setStyleSheet(QStringLiteral(
        "QLabel { color:#10B981; font-size:28px; font-weight:900; }"));
    healthLayout->addWidget(healthText);
    healthLayout->addStretch();
    healthLayout->addWidget(m_healthScoreLabel);
    visualLayout->addWidget(healthCard);

    auto *ruleLabel = new QLabel(QStringLiteral("计算规则： (空闲 + 使用中) / 总数 × 100%"), visualCard);
    ruleLabel->setStyleSheet(QStringLiteral(
        "QLabel { color:#6F8190; font-size:12px; font-weight:650; }"));
    visualLayout->addWidget(ruleLabel);

    contentLayout->addWidget(visualCard, 6);
    rootLayout->addLayout(contentLayout, 1);
}


void MainWindow::refreshChargerStatusOverview()
{
    if (!m_statusChartView || !m_statusTable || !m_healthScoreLabel) {
        return;
    }

    StatsService::ChargerStatusSummary summary;
    QString errorMessage;
    if (!m_statsService.loadChargerStatusSummary(summary, errorMessage)) {
        m_statusTable->setItem(0, 0, new QTableWidgetItem(QStringLiteral("在用")));
        m_statusTable->setItem(1, 0, new QTableWidgetItem(QStringLiteral("闲置")));
        m_statusTable->setItem(2, 0, new QTableWidgetItem(QStringLiteral("故障")));
        for (int row = 0; row < 3; ++row) {
            m_statusTable->setItem(row, 1, new QTableWidgetItem(QStringLiteral("--")));
            m_statusTable->setItem(row, 2, new QTableWidgetItem(QStringLiteral("--")));
        }
        m_healthScoreLabel->setText(QStringLiteral("--"));
        m_statusQueryPerformanceLabel->setText(QStringLiteral("统计失败"));
        m_statusEmptyLabel->setText(errorMessage);
        m_statusEmptyLabel->setVisible(true);
        return;
    }

    auto setRow = [this](int row, const QString &name, int count, double percent, const QString &valueColor) {
        auto *statusItem = new QTableWidgetItem(name);
        statusItem->setForeground(QBrush(QColor(valueColor)));
        statusItem->setFont(QFont(QStringLiteral("Sans Serif"), 14, QFont::Bold));
        auto *countItem = new QTableWidgetItem(QString::number(count));
        countItem->setTextAlignment(Qt::AlignCenter);
        auto *percentItem = new QTableWidgetItem(QStringLiteral("%1%").arg(percent, 0, 'f', 2));
        percentItem->setTextAlignment(Qt::AlignCenter);
        m_statusTable->setItem(row, 0, statusItem);
        m_statusTable->setItem(row, 1, countItem);
        m_statusTable->setItem(row, 2, percentItem);
    };

    const double total = static_cast<double>(summary.total);
    const double inUsePercent = total > 0.0 ? summary.inUse / total * 100.0 : 0.0;
    const double idlePercent = total > 0.0 ? summary.idle / total * 100.0 : 0.0;
    const double faultPercent = total > 0.0 ? summary.fault / total * 100.0 : 0.0;

    setRow(0, QStringLiteral("在用"), summary.inUse, inUsePercent, QStringLiteral("#F6A648"));
    setRow(1, QStringLiteral("闲置"), summary.idle, idlePercent, QStringLiteral("#10B981"));
    setRow(2, QStringLiteral("故障"), summary.fault, faultPercent, QStringLiteral("#F05252"));

    m_healthScoreLabel->setText(QStringLiteral("%1%").arg(summary.healthPercent, 0, 'f', 2));
    m_statusQueryPerformanceLabel->setText(QStringLiteral("统计耗时 %1 ms").arg(summary.elapsedMs));

    rebuildChargerStatusChart(summary);
}


void MainWindow::rebuildChargerStatusChart(
    const StatsService::ChargerStatusSummary &summary)
{
    auto *chart = new QChart();
    chart->setBackgroundBrush(QBrush(QColor(QStringLiteral("#121D30"))));
    chart->setPlotAreaBackgroundVisible(false);
    chart->setMargins(QMargins(6, 2, 6, 2));
    // The default QPieSeries legend becomes cramped on a wide dashboard,
    // especially when status names and counts are combined. Keep the chart
    // legend hidden and use the slice labels themselves as the visual key.
    chart->legend()->setVisible(false);

    auto *series = new QPieSeries(chart);
    series->setHoleSize(0.53);

    const int total = summary.total;
    auto addSlice = [series, total](const QString &label, int count, const QColor &color) {
        auto *slice = series->append(label, count);
        slice->setColor(color);
        slice->setBorderColor(QColor(QStringLiteral("#121D30")));
        slice->setBorderWidth(2);
        if (total > 0 && count > 0) {
            slice->setLabelVisible(true);
            slice->setLabelColor(QColor(QStringLiteral("#F4F7F8")));
            slice->setLabelPosition(QPieSlice::LabelOutside);
            // Make each slice self-describing: status name + percentage + count.
            slice->setLabel(QStringLiteral("%1\n%2% · %3").arg(label)
                                .arg(static_cast<double>(count) / total * 100.0, 0, 'f', 1)
                                .arg(count));
            slice->setExploded(false);
        }
    };

    addSlice(QStringLiteral("在用"), summary.inUse, QColor(QStringLiteral("#F6A648")));
    addSlice(QStringLiteral("闲置"), summary.idle, QColor(QStringLiteral("#10B981")));
    addSlice(QStringLiteral("故障"), summary.fault, QColor(QStringLiteral("#F05252")));

    chart->addSeries(series);

    if (total == 0) {
        m_statusEmptyLabel->setVisible(true);
        m_statusEmptyLabel->setText(QStringLiteral("暂无数据"));
    } else {
        m_statusEmptyLabel->setVisible(false);
    }

    m_statusChartView->setChart(chart);
}


