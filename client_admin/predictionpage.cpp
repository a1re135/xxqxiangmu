#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "mainwindowhelpers.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListView>
#include <QPainter>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QBrush>
#include <QColor>
#include <QDateTime>
#include <QMessageBox>
#include <QPen>
#include <QSignalBlocker>
#include <QTableWidgetItem>
#include <QTabBar>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QFile>
#include <QTabWidget>
#include <QTextStream>

#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>

namespace {

void rebuildPredictionChart(
    QChartView *chartView,
    const PredictionService::PredictionSummary &summary)
{
    if (!chartView) {
        return;
    }

    auto *chart = new QChart();
    styleChart(chart);

    auto *series = new QLineSeries(chart);

    QPen linePen(QColor(QStringLiteral("#10B981")));
    linePen.setWidth(3);

    series->setPen(linePen);
    series->setPointsVisible(true);
    series->setPointLabelsVisible(false);

    double maxLoad = 0.0;

    for (const auto &point : summary.points) {

        series->append(
            static_cast<qreal>(
                point.targetTime.toMSecsSinceEpoch()
            ),
            point.predictedLoad
        );

        maxLoad =
            qMax(
                maxLoad,
                point.predictedLoad
            );
    }

    chart->addSeries(series);

    // ------------------------------------------
    // X axis - date
    // ------------------------------------------

    auto *axisX =
        new QDateTimeAxis(chart);

    axisX->setFormat(
        QStringLiteral("MM-dd HH:mm")
    );

    axisX->setTitleText(
        QStringLiteral("时间")
    );

    axisX->setLabelsColor(
        QColor(QStringLiteral("#8FA1AC"))
    );

    axisX->setTitleBrush(
        QBrush(
            QColor(
                QStringLiteral("#8FA1AC")
            )
        )
    );

    axisX->setGridLineColor(
        QColor(QStringLiteral("#25364A"))
    );

    if (!summary.points.isEmpty()) {

        const QDateTime first =
            summary.points.first().targetTime;

        const QDateTime last =
            summary.points.last().targetTime;

        axisX->setRange(
            first.addSecs(-43200),
            last.addSecs(43200)
        );
    }

    const int ticks =
        qBound(
            2,
            summary.points.size() + 1,
            8
        );

    axisX->setTickCount(ticks);


    // ------------------------------------------
    // Y axis - predicted kWh
    // ------------------------------------------

    auto *axisY =
        new QValueAxis(chart);

    axisY->setTitleText(
        QStringLiteral("预计负荷 (kWh)")
    );

    axisY->setLabelFormat(
        QStringLiteral("%.1f")
    );

    axisY->setLabelsColor(
        QColor(QStringLiteral("#8FA1AC"))
    );

    axisY->setTitleBrush(
        QBrush(
            QColor(
                QStringLiteral("#8FA1AC")
            )
        )
    );

    axisY->setGridLineColor(
        QColor(QStringLiteral("#25364A"))
    );

    axisY->setRange(
        0.0,
        qMax(
            1.0,
            maxLoad * 1.20
        )
    );

    chart->addAxis(
        axisX,
        Qt::AlignBottom
    );

    chart->addAxis(
        axisY,
        Qt::AlignLeft
    );

    series->attachAxis(axisX);
    series->attachAxis(axisY);

    chart->legend()->setVisible(false);

    chartView->setChart(chart);
}

} // namespace

void MainWindow::setupPredictionPage()
{
    auto *page =
        uiObject<QWidget>(
            this,
            QStringLiteral("predictionPage")
        );

    auto *rootLayout =
        uiObject<QVBoxLayout>(
            this,
            QStringLiteral("predictionPageLayout")
        );

    if (!page || !rootLayout) {
        return;
    }

    // Remove the placeholder content from mainwindow.ui
    while (rootLayout->count() > 0) {

        QLayoutItem *item =
            rootLayout->takeAt(0);

        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }

        delete item;
    }

    rootLayout->setContentsMargins(
        28, 24, 28, 24
    );

    rootLayout->setSpacing(14);

    // ==================================================
    // Controls
    // ==================================================

    auto *controlCard =
        new QFrame(page);

    controlCard->setStyleSheet(QStringLiteral(
        "QFrame {"
        "background:#121D30;"
        "border:1px solid #25364A;"
        "border-radius:14px;"
        "}"
    ));

    auto *controlLayout =
        new QHBoxLayout(controlCard);

    controlLayout->setContentsMargins(
        16, 12, 16, 12
    );

    controlLayout->setSpacing(10);


    auto *stationLabel =
        new QLabel(
            QStringLiteral("电站"),
            controlCard
        );

    stationLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "color:#8FA1AC;"
        "font-size:13px;"
        "font-weight:800;"
        "}"
    ));

    m_predictionStationCombo =
        new QComboBox(controlCard);

    m_predictionStationCombo
        ->setMinimumWidth(260);

    m_predictionStationCombo
        ->setMinimumHeight(40);


    // Custom dropdown so text remains readable
    auto *comboView =
        new QListView(m_predictionStationCombo);

    comboView->setFrameShape(QFrame::NoFrame);
    comboView->setStyleSheet(QStringLiteral(R"(

        QListView {
            background:#10233B;
            color:#EAF3FF;
            border:1px solid #294B6D;
            outline:none;
            font-size:13px;
            font-weight:700;
        }

        QListView::item {
            min-height:32px;
            padding:6px 10px;
            color:#EAF3FF;
            background:#10233B;
        }

        QListView::item:hover {
            background:#173656;
            color:#FFFFFF;
        }

        QListView::item:selected {
            background:#153B35;
                color:#FFFFFF;
        }

    )"));

    m_predictionStationCombo->setView(comboView);

    m_predictionStationCombo->setStyleSheet(QStringLiteral(R"(

        QComboBox {
            background:#10233B;
            color:#EAF3FF;
            border:1px solid #294B6D;
            border-radius:10px;
            padding:7px 12px;
            font-size:13px;
            font-weight:700;
        }

        QComboBox:hover {
            border-color:#10B981;
        }

        QComboBox:focus {
            border-color:#10B981;
        }

        QComboBox::drop-down {
            border:none;
            width:28px;
        }

        QComboBox::down-arrow {
            image: none;
        }

    )"));


    auto *periodLabel =
        new QLabel(
            QStringLiteral("预测周期"),
            controlCard
        );

    periodLabel->setStyleSheet(
        stationLabel->styleSheet()
    );


    const QString periodStyle =
        QStringLiteral(R"(

        QPushButton {
            background:#10233B;
            color:#9FB3C8;

            border:1px solid #294B6D;
            border-radius:9px;

            padding:8px 16px;

            font-size:13px;
            font-weight:800;
        }

        QPushButton:hover {
            color:#FFFFFF;
            border-color:#10B981;
        }

        QPushButton:checked {
            background:#123D34;
            color:#34D399;
            border:1px solid #1F6A58;
        }

    )");


    m_prediction1Button =
        new QPushButton(
            QStringLiteral("未来 1 小时"),
            controlCard
        );

    m_prediction6Button =
        new QPushButton(
            QStringLiteral("未来 6 小时"),
            controlCard
        );

    m_prediction24Button =
        new QPushButton(
            QStringLiteral("未来 24 小时"),
            controlCard
        );

    m_prediction1Button->setCheckable(true);
    m_prediction6Button->setCheckable(true);
    m_prediction24Button->setCheckable(true);

    m_prediction1Button->setAutoExclusive(true);
    m_prediction6Button->setAutoExclusive(true);
    m_prediction24Button->setAutoExclusive(true);

    // Default = 6 hours
    m_prediction6Button->setChecked(true);

    m_prediction1Button->setStyleSheet(periodStyle);
    m_prediction6Button->setStyleSheet(periodStyle);
    m_prediction24Button->setStyleSheet(periodStyle);


    m_runPredictionButton =
        new QPushButton(
            QStringLiteral("↻ 重新运行预测"),
            controlCard
        );

    m_runPredictionButton
        ->setMinimumHeight(40);

    m_runPredictionButton
        ->setStyleSheet(QStringLiteral(R"(

        QPushButton {
            background:#10B981;
            color:#071A15;

            border:none;
            border-radius:10px;

            padding:8px 18px;

            font-size:13px;
            font-weight:900;
        }

        QPushButton:hover {
            background:#16D497;
        }

        QPushButton:pressed {
            background:#0D9C6D;
        }

    )"));


    controlLayout->addWidget(stationLabel);
    controlLayout->addWidget(
        m_predictionStationCombo
    );

    controlLayout->addSpacing(12);

    controlLayout->addWidget(periodLabel);

    controlLayout->addWidget(m_prediction1Button);
    controlLayout->addWidget(m_prediction6Button);
    controlLayout->addWidget(m_prediction24Button);

    controlLayout->addStretch();

    controlLayout->addWidget(
        m_runPredictionButton
    );

    rootLayout->addWidget(controlCard);


    // ==================================================
    // Metric cards
    // ==================================================

    auto *metrics =
        new QGridLayout();

    metrics->setSpacing(14);


    auto makeMetricCard =
        [page](
            const QString &caption,
            QLabel *&valueLabel)
    {
        auto *card =
            new QFrame(page);

        card->setMinimumHeight(100);

        card->setStyleSheet(QStringLiteral(
            "QFrame {"
            "background:#162336;"
            "border:1px solid #2B3A4D;"
            "border-radius:14px;"
            "}"
        ));

        auto *layout =
            new QVBoxLayout(card);

        layout->setContentsMargins(
            18, 14, 18, 14
        );

        layout->setSpacing(5);

        auto *label =
            new QLabel(
                caption,
                card
            );

        label->setStyleSheet(QStringLiteral(
            "QLabel {"
            "color:#8FA1AC;"
            "font-size:13px;"
            "font-weight:800;"
            "}"
        ));

        valueLabel =
            new QLabel(
                QStringLiteral("--"),
                card
            );

        valueLabel->setStyleSheet(QStringLiteral(
            "QLabel {"
            "color:#F5F7FA;"
            "font-size:25px;"
            "font-weight:900;"
            "}"
        ));

        layout->addWidget(label);
        layout->addWidget(valueLabel);

        return card;
    };


    metrics->addWidget(
        makeMetricCard(
            QStringLiteral("预计总负荷"),
            m_predictionTotalLabel
        ),
        0, 0
    );

    metrics->addWidget(
        makeMetricCard(
            QStringLiteral("平均小时负荷"),
            m_predictionAverageLabel
        ),
        0, 1
    );

    metrics->addWidget(
        makeMetricCard(
            QStringLiteral("预计高峰时间"),
            m_predictionPeakLabel
        ),
        0, 2
    );

    rootLayout->addLayout(metrics);


    // ==================================================
    // Prediction chart
    // ==================================================

    auto *chartCard =
        new QFrame(page);

    chartCard->setMinimumHeight(190);

    chartCard->setStyleSheet(QStringLiteral(
        "QFrame {"
        "background:#121D30;"
        "border:1px solid #25364A;"
        "border-radius:14px;"
        "}"
    ));

    auto *chartLayout =
        new QVBoxLayout(chartCard);

    chartLayout->setContentsMargins(
        14, 12, 14, 12
    );

    auto *chartTitle =
        new QLabel(
            QStringLiteral("未来充电负荷趋势"),
            chartCard
        );

    chartTitle->setStyleSheet(QStringLiteral(
        "QLabel {"
        "color:#F3F6F8;"
        "font-size:18px;"
        "font-weight:850;"
        "}"
    ));

    chartLayout->addWidget(chartTitle);

    m_predictionChartView =
        new QChartView(chartCard);

    m_predictionChartView
        ->setRenderHint(
            QPainter::Antialiasing
        );

    m_predictionChartView
        ->setStyleSheet(
            QStringLiteral(
                "background:transparent;"
                "border:none;"
            )
        );

    auto *emptyChart =
        new QChart();

    styleChart(emptyChart);

    m_predictionChartView
        ->setChart(emptyChart);

    chartLayout->addWidget(
        m_predictionChartView,
        1
    );


    // ==================================================
    // Prediction table
    // ==================================================

    auto *tableCard =
        new QFrame(page);

    tableCard->setMinimumHeight(170);

    tableCard->setStyleSheet(
        chartCard->styleSheet()
    );

    auto *tableLayout =
        new QVBoxLayout(tableCard);

    tableLayout->setContentsMargins(
        14, 12, 14, 12
    );

    auto *tableTitle =
        new QLabel(
            QStringLiteral("预测明细"),
            tableCard
        );

    tableTitle->setStyleSheet(
        chartTitle->styleSheet()
    );

    tableLayout->addWidget(tableTitle);


    m_predictionTable =
        new QTableWidget(
            0,
            4,
            tableCard
        );

    m_predictionTable
        ->setHorizontalHeaderLabels({
            QStringLiteral("日期"),
            QStringLiteral("预计负荷(kWh)"),
            QStringLiteral("预计空闲桩"),
            QStringLiteral("峰值状态")
        });

    m_predictionTable
        ->verticalHeader()
        ->setVisible(false);

    m_predictionTable
        ->setEditTriggers(
            QAbstractItemView::NoEditTriggers
        );

    m_predictionTable
        ->setSelectionBehavior(
            QAbstractItemView::SelectRows
        );

    m_predictionTable
        ->setShowGrid(false);

    m_predictionTable
        ->verticalHeader()
        ->setDefaultSectionSize(34);

    m_predictionTable
        ->horizontalHeader()
        ->setSectionResizeMode(
            QHeaderView::Stretch
        );

    m_predictionTable
        ->setStyleSheet(QStringLiteral(R"(

        QTableWidget {
            background:#101A2B;
            color:#EAF0F3;

            border:1px solid #27394D;
            border-radius:10px;

            font-size:13px;
            font-weight:600;

            selection-background-color:#153B35;
            selection-color:#F4F7F8;
        }

        QHeaderView::section {
            background:#1A2739;
            color:#8FA1AC;

            border:none;

            padding:9px 7px;

            font-size:12px;
            font-weight:800;
        }

        QTableWidget::item {
            padding:7px;
            border-bottom:1px solid #1D2B3C;
        }

    )"));

    tableLayout->addWidget(
        m_predictionTable,
        1
    );


    // ==================================================
    // Splitter
    // ==================================================

    auto *splitter =
        new QSplitter(
            Qt::Vertical,
            page
        );

    splitter
        ->setChildrenCollapsible(false);

    splitter
        ->setHandleWidth(6);

    splitter->setStyleSheet(QStringLiteral(
        "QSplitter::handle {"
        "background:#1B2C3D;"
        "border-radius:3px;"
        "margin:2px 80px;"
        "}"
        "QSplitter::handle:hover {"
        "background:#10B981;"
        "}"
    ));

    splitter->addWidget(chartCard);
    splitter->addWidget(tableCard);

    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);

    splitter->setSizes(
        QList<int>() << 280 << 190
    );

    auto *tabs = new QTabWidget(page);

    tabs->setStyleSheet(QStringLiteral(R"(

        QTabWidget {
            background: transparent;
        }

        QTabWidget::pane {
            border: none;
            background: transparent;
            margin-top: 10px;
        }

        QTabBar {
            background: transparent;
        }

        QTabBar::tab {
            min-width: 105px;
            min-height: 34px;

            background: #13243A;
            color: #AFC3D8;

            border: 1px solid #294B6D;
            border-radius: 9px;

            margin-right: 8px;
            padding: 0px 16px;

            font-size: 13px;
            font-weight: 800;
        }

        QTabBar::tab:hover {
            background: #173656;
            color: #FFFFFF;
            border-color: #3B638A;
        }

        QTabBar::tab:selected {
            background: #123F37;
            color: #34D399;

            border: 1px solid #1F806A;
        }

        QTabBar::tab:selected:hover {
            background: #165044;
            color: #4ADEA8;
        }

    )"));

    tabs->tabBar()->setExpanding(false);

    auto *forecastTab =
        new QWidget(tabs);

    auto *forecastLayout =
        new QVBoxLayout(forecastTab);

    forecastLayout->setContentsMargins(
        8, 8, 8, 8
    );

    forecastLayout->addWidget(
        splitter,
        1
    );

    tabs->addTab(
        forecastTab,
        QStringLiteral("未来预测")
    );

    auto *evaluationTab =
        new QWidget(tabs);

    auto *evaluationLayout =
        new QVBoxLayout(evaluationTab);

    evaluationLayout->setContentsMargins(
        12, 12, 12, 12
    );

    evaluationLayout->setSpacing(10);

    auto *evaluationActionRow =
        new QHBoxLayout();

    auto *evaluationDescription =
        new QLabel(
            QStringLiteral(
                "使用历史数据比较实际负荷与"
                "RandomForest预测负荷"
            ),
            evaluationTab
        );

    evaluationDescription->setStyleSheet(
        QStringLiteral(
            "color:#8FA1AC;"
            "font-size:13px;"
            "font-weight:600;"
        )
    );

    m_runEvaluationButton =
        new QPushButton(
            QStringLiteral("运行模型回测"),
            evaluationTab
        );

    m_runEvaluationButton->setStyleSheet(
        QStringLiteral(R"(

            QPushButton {
                background:#10233B;
                color:#34D399;

                border:1px solid #1F6A58;
                border-radius:9px;

                padding:8px 16px;

                font-weight:800;
            }

            QPushButton:hover {
                background:#153B35;
            }

        )")
    );

    evaluationActionRow->addWidget(
        evaluationDescription
    );

    evaluationActionRow->addStretch();

    evaluationActionRow->addWidget(
        m_runEvaluationButton
    );

    evaluationLayout->addLayout(
        evaluationActionRow
    );

    m_evaluationChartView =
        new QChartView(evaluationTab);

    m_evaluationChartView->setFrameShape(
        QFrame::NoFrame
    );

    m_evaluationChartView->setStyleSheet(
        QStringLiteral(
            "QChartView {"
            "background:#0F1C2E;"
            "border:none;"
            "}"
        )
    );

    m_evaluationChartView->setRenderHint(
        QPainter::Antialiasing
    );

    m_evaluationChartView->setMinimumHeight(
        220
    );

    auto *emptyEvaluationChart =
        new QChart();

    styleChart(emptyEvaluationChart);

    m_evaluationChartView->setChart(
        emptyEvaluationChart
    );

    evaluationLayout->addWidget(
        m_evaluationChartView,
        1
    );

    m_evaluationTable =
        new QTableWidget(
            0,
            4,
            evaluationTab
        );

    m_evaluationTable->setFrameShape(
        QFrame::NoFrame
    );

    m_evaluationTable->setShowGrid(true);

    m_evaluationTable->setHorizontalHeaderLabels({
        QStringLiteral("时间"),
        QStringLiteral("实际负荷(kWh)"),
        QStringLiteral("预测负荷(kWh)"),
        QStringLiteral("绝对误差(kWh)")
    });

    m_evaluationTable
        ->horizontalHeader()
        ->setSectionResizeMode(
            QHeaderView::Stretch
        );

    m_evaluationTable
        ->verticalHeader()
        ->setVisible(false);

    m_evaluationTable
        ->setEditTriggers(
            QAbstractItemView::NoEditTriggers
        );

    m_evaluationTable->setStyleSheet(QStringLiteral(R"(

        QTableWidget {
            background: #0E1A2B;
            alternate-background-color: #101F32;

            color: #EAF3FF;

            border: none;
            outline: none;

            gridline-color: #20344A;

            selection-background-color: #123F37;
            selection-color: #FFFFFF;

            font-size: 12px;
            font-weight: 700;
        }

        QTableWidget::item {
            border: none;
            padding: 6px;
        }

        QTableWidget::item:selected {
            background: #123F37;
            color: #FFFFFF;
        }

        QHeaderView {
            background: transparent;
            border: none;
        }

        QHeaderView::section {
            background: #1A2B40;
            color: #AFC3D8;

            border: none;
            border-right: 1px solid #263C54;
            border-bottom: 1px solid #263C54;

            padding: 8px;

            font-size: 12px;
            font-weight: 800;
        }

        QTableCornerButton::section {
            background: #1A2B40;
            border: none;
        }

        QScrollBar:vertical {
            background: #101B2B;
            width: 10px;
            margin: 0px;
            border: none;
        }

        QScrollBar::handle:vertical {
            background: #294B6D;
            min-height: 25px;
            border-radius: 5px;
        }

        QScrollBar::handle:vertical:hover {
            background: #3B638A;
        }

        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0px;
        }

        QScrollBar:horizontal {
            background: #101B2B;
            height: 10px;
            margin: 0px;
            border: none;
        }

        QScrollBar::handle:horizontal {
            background: #294B6D;
            min-width: 25px;
            border-radius: 5px;
        }

        QScrollBar::add-line:horizontal,
        QScrollBar::sub-line:horizontal {
            width: 0px;
        }

    )"));

    evaluationLayout->addWidget(
        m_evaluationTable
    );

    tabs->addTab(
        evaluationTab,
        QStringLiteral("模型回测")
    );

    rootLayout->addWidget(
        tabs,
        1
    );
    // ==================================================
    // Signals
    // ==================================================

    connect(
        m_prediction1Button,
        &QPushButton::clicked,
        this,
        [this]() {
            m_predictionHours = 1;
            refreshPredictionPage();
        }
    );

    connect(
        m_prediction6Button,
        &QPushButton::clicked,
        this,
        [this]() {
            m_predictionHours = 6;
            refreshPredictionPage();
        }
    );

    connect(
        m_prediction24Button,
        &QPushButton::clicked,
        this,
        [this]() {
            m_predictionHours = 24;
            refreshPredictionPage();
        }
    );

    connect(
        m_runPredictionButton,
        &QPushButton::clicked,
        this,
        &MainWindow::runPrediction
    );

    connect(
        m_runEvaluationButton,
        &QPushButton::clicked,
        this,
        &MainWindow::runPredictionEvaluation
    );
}


void MainWindow::refreshPredictionPage()
{
    if (!m_predictionStationCombo
        || !m_predictionTotalLabel
        || !m_predictionAverageLabel
        || !m_predictionPeakLabel
        || !m_predictionChartView
        || !m_predictionTable) {

        return;
    }


    // ==================================================
    // 1. Remember the currently selected station
    // ==================================================

    const int previousStationId =
        m_predictionStationCombo
            ->currentData()
            .toInt();


    // ==================================================
    // 2. Load station list
    // ==================================================

    QList<AdminStationService::StationRecord>
        stations;

    QString stationError;

    if (!m_stationService.loadStations(
            stations,
            stationError)) {

        QMessageBox::warning(
            this,
            QStringLiteral("智能预测"),
            stationError
        );

        return;
    }


    // Prevent currentIndexChanged from firing while
    // rebuilding the combo box.
    const QSignalBlocker blocker(
        m_predictionStationCombo
    );

    m_predictionStationCombo->clear();

    int restoreIndex = -1;

    for (int i = 0;
         i < stations.size();
         ++i) {

        const auto &station =
            stations.at(i);

        m_predictionStationCombo
            ->addItem(
                station.name,
                station.id
            );

        if (station.id ==
            previousStationId) {

            restoreIndex = i;
        }
    }


    if (restoreIndex >= 0) {

        m_predictionStationCombo
            ->setCurrentIndex(
                restoreIndex
            );

    } else if (
        m_predictionStationCombo
            ->count() > 0) {

        m_predictionStationCombo
            ->setCurrentIndex(0);
    }


    // ==================================================
    // 3. No station available
    // ==================================================

    if (m_predictionStationCombo
            ->count() == 0) {

        m_predictionTotalLabel
            ->setText(
                QStringLiteral("--")
            );

        m_predictionAverageLabel
            ->setText(
                QStringLiteral("--")
            );

        m_predictionPeakLabel
            ->setText(
                QStringLiteral("--")
            );

        m_predictionTable
            ->setRowCount(0);

        auto *emptyChart =
            new QChart();

        styleChart(emptyChart);

        m_predictionChartView
            ->setChart(emptyChart);

        return;
    }


    // ==================================================
    // 4. Determine selected station
    // ==================================================

    const int stationId =
        m_predictionStationCombo
            ->currentData()
            .toInt();

    if (stationId <= 0) {
        return;
    }


    // ==================================================
    // 5. Load most recent prediction
    // ==================================================

    PredictionService::PredictionSummary
        summary;

    QString predictionError;

    const bool loaded =
        m_predictionService
            .loadLatestPrediction(
                stationId,
                m_predictionHours,
                summary,
                predictionError
            );


    // ==================================================
    // 6. Prediction failed
    // ==================================================

    if (!loaded
        || summary.points.size() < m_predictionHours) {

        m_predictionTotalLabel
            ->setText(QStringLiteral("--"));

        m_predictionAverageLabel
            ->setText(QStringLiteral("--"));

        m_predictionPeakLabel
            ->setText(QStringLiteral("--"));

        m_predictionTable
            ->setRowCount(0);

        auto *emptyChart =
            new QChart();

        styleChart(emptyChart);

        m_predictionChartView
            ->setChart(emptyChart);

        return;
    }

    // ==================================================
    // 7. Update metric cards
    // ==================================================

    m_predictionTotalLabel->setText(
        QStringLiteral("%1 kWh")
            .arg(
                summary.totalPredictedLoad,
                0,
                'f',
                2
            )
    );


    m_predictionAverageLabel->setText(
        QStringLiteral("%1 kWh/小时")
            .arg(
                summary.averageDailyLoad,
                0,
                'f',
                2
            )
    );


    if (summary.peakTime.isValid()) {

        m_predictionPeakLabel->setText(
            QStringLiteral(
                "%1 · %2 kWh"
            )
                .arg(
                    summary.peakTime.toString(
                        QStringLiteral(
                            "MM-dd HH:mm"
                        )
                    )
                )
                .arg(
                    summary.peakLoad,
                    0,
                    'f',
                    2
                )
        );

        m_predictionPeakLabel->setStyleSheet(
            QStringLiteral(
                "QLabel {"
                "color:#F87171;"
                "font-size:25px;"
                "font-weight:900;"
                "}"
            )
        );

    } else {

        m_predictionPeakLabel->setText(
            QStringLiteral("--")
        );

        m_predictionPeakLabel->setStyleSheet(
            QStringLiteral(
                "QLabel {"
                "color:#F5F7FA;"
                "font-size:25px;"
                "font-weight:900;"
                "}"
            )
        );
    }


    // ==================================================
    // 8. Rebuild chart
    // ==================================================

    rebuildPredictionChart(
        m_predictionChartView,
        summary
    );


    // ==================================================
    // 9. Rebuild prediction table
    // ==================================================

    m_predictionTable->setRowCount(0);

    for (const auto &point :
         summary.points) {

        const int row =
            m_predictionTable
                ->rowCount();

        m_predictionTable
            ->insertRow(row);


        auto addItem =
            [this, row](
                int column,
                const QString &text)
        {
            auto *item =
                new QTableWidgetItem(
                    text
                );

            item->setTextAlignment(
                Qt::AlignCenter
            );

            m_predictionTable
                ->setItem(
                    row,
                    column,
                    item
                );

            return item;
        };


        addItem(
            0,
            point.targetTime
                .toString(
                        QStringLiteral("yyyy-MM-dd HH:mm")
                    )
        );


        addItem(
            1,
            QString::number(
                point.predictedLoad,
                'f',
                2
            )
        );


        addItem(
            2,
            QString::number(
                point.predictedFreeChargers
            )
        );


        auto *peakItem =
            addItem(
                3,
                point.isPeak
                    ? QStringLiteral("高峰预警")
                    : QStringLiteral("正常")
            );

        peakItem->setForeground(
            QBrush(
                QColor(
                    point.isPeak
                        ? QStringLiteral("#F87171")
                        : QStringLiteral("#34D399")
                )
            )
        );


        // ==========================================
        // Highlight entire peak row
        // ==========================================

        if (point.isPeak) {

            for (int column = 0;
                 column < m_predictionTable->columnCount();
                 ++column) {

                QTableWidgetItem *item =
                    m_predictionTable->item(
                        row,
                        column
                    );

                if (!item) {
                    continue;
                }

                item->setBackground(
                    QBrush(
                        QColor(
                            QStringLiteral(
                                "#2D1D27"
                            )
                        )
                    )
                );
            }
        }
    }
}


void MainWindow::runPrediction()
{
    if (!m_predictionStationCombo
        || !m_runPredictionButton) {
        return;
    }

    const int stationId =
        m_predictionStationCombo
            ->currentData()
            .toInt();

    if (stationId <= 0) {
        QMessageBox::information(
            this,
            QStringLiteral("智能预测"),
            QStringLiteral("请先选择一个充电站。")
        );

        return;
    }

    if (m_predictionProcess
        && m_predictionProcess->state()
            != QProcess::NotRunning) {

        QMessageBox::information(
            this,
            QStringLiteral("智能预测"),
            QStringLiteral("预测任务正在运行，请稍候。")
        );

        return;
    }


    // ==========================================
    // Project paths
    // ==========================================

    const QString projectRoot =
        QStringLiteral(NCS_PROJECT_ROOT);

    const QString scriptPath =
        QDir(projectRoot)
            .filePath(
                QStringLiteral(
                    "ml/main.py"
                )
            );


#ifdef Q_OS_WIN

    QString pythonPath =
        QDir(projectRoot)
            .filePath(
                QStringLiteral(
                    ".venv/Scripts/python.exe"
                )
            );

#else

    QString pythonPath =
        QDir(projectRoot)
            .filePath(
                QStringLiteral(
                    ".venv/bin/python"
                )
            );

#endif


    // Fallback if the virtual environment
    // cannot be found.
    if (!QFileInfo::exists(pythonPath)) {

#ifdef Q_OS_WIN
        pythonPath =
            QStringLiteral("python");
#else
        pythonPath =
            QStringLiteral("python3");
#endif
    }


    if (!QFileInfo::exists(scriptPath)) {

        QMessageBox::warning(
            this,
            QStringLiteral("预测失败"),
            QStringLiteral(
                "找不到 Python 预测脚本：\n%1"
            ).arg(scriptPath)
        );

        return;
    }


    // ==========================================
    // Start process
    // ==========================================

    m_runPredictionButton
        ->setEnabled(false);

    m_runPredictionButton
        ->setText(
            QStringLiteral("正在重新训练并预测...")
        );


    m_predictionProcess =
        new QProcess(this);

    m_predictionProcess
        ->setWorkingDirectory(
            projectRoot
        );


    QStringList arguments;

    arguments
        << scriptPath
        << QStringLiteral("refresh")
        << QStringLiteral("--station")
        << QString::number(
               stationId
           )
        << QStringLiteral("--horizon")
        << QString::number(
               m_predictionHours
           );


    connect(
        m_predictionProcess,
        &QProcess::finished,
        this,
        [this](
            int exitCode,
            QProcess::ExitStatus exitStatus)
        {
            const QString output =
                QString::fromUtf8(
                    m_predictionProcess
                        ->readAllStandardOutput()
                );

            const QString errorOutput =
                QString::fromUtf8(
                    m_predictionProcess
                        ->readAllStandardError()
                );


            m_runPredictionButton
                ->setEnabled(true);

            m_runPredictionButton
                ->setText(
                    QStringLiteral(
                        "↻ 重新运行预测"
                    )
                );


            if (exitStatus != QProcess::NormalExit
                || exitCode != 0) {

                QMessageBox::warning(
                    this,
                    QStringLiteral("预测失败"),
                    QStringLiteral(
                        "重新训练或预测失败。\n\n%1"
                    ).arg(
                        errorOutput.isEmpty()
                            ? output
                            : errorOutput
                    )
                );

                m_predictionProcess->deleteLater();
                m_predictionProcess = nullptr;

                return;
            }


            // Python already wrote results
            // into load_prediction.
            refreshPredictionPage();


            QMessageBox::information(
                this,
                QStringLiteral("预测完成"),
                QStringLiteral(
                    "模型已重新训练，"
                    "并完成未来 %1 小时预测。"
                ).arg(
                    m_predictionHours
                )
            );


            m_predictionProcess
                ->deleteLater();

            m_predictionProcess =
                nullptr;
        }
    );


    connect(
        m_predictionProcess,
        &QProcess::errorOccurred,
        this,
        [this](
            QProcess::ProcessError)
        {
            if (!m_predictionProcess) {
                return;
            }

            m_runPredictionButton
                ->setEnabled(true);

            m_runPredictionButton
                ->setText(
                    QStringLiteral(
                        "↻ 重新运行预测"
                    )
                );
        }
    );


    m_predictionProcess->start(
        pythonPath,
        arguments
    );
}

void MainWindow::runPredictionEvaluation()
{
    if (!m_predictionStationCombo
        || !m_runEvaluationButton) {
        return;
    }

    const int stationId =
        m_predictionStationCombo
            ->currentData()
            .toInt();

    if (stationId <= 0) {
        return;
    }

    if (m_evaluationProcess
        && m_evaluationProcess->state()
            != QProcess::NotRunning) {
        return;
    }

    const QString projectRoot =
        QStringLiteral(NCS_PROJECT_ROOT);

    const QString scriptPath =
        QDir(projectRoot)
            .filePath(
                QStringLiteral(
                    "ml/evaluate_model.py"
                )
            );

    const QString outputPath =
        QDir(projectRoot)
            .filePath(
                QStringLiteral(
                    "ml/evaluation_results.csv"
                )
            );


#ifdef Q_OS_WIN

    QString pythonPath =
        QDir(projectRoot)
            .filePath(
                QStringLiteral(
                    ".venv/Scripts/python.exe"
                )
            );

#else

    QString pythonPath =
        QDir(projectRoot)
            .filePath(
                QStringLiteral(
                    ".venv/bin/python"
                )
            );

#endif


    if (!QFileInfo::exists(pythonPath)) {

#ifdef Q_OS_WIN
        pythonPath = QStringLiteral("python");
#else
        pythonPath = QStringLiteral("python3");
#endif

    }


    m_runEvaluationButton->setEnabled(false);

    m_runEvaluationButton->setText(
        QStringLiteral("正在运行回测...")
    );


    m_evaluationProcess =
        new QProcess(this);

    m_evaluationProcess->setWorkingDirectory(
        projectRoot
    );


    // Evaluation makes most sense with at least
    // six historical hours.
    const int evaluationHours =
        m_predictionHours == 1
            ? 6
            : m_predictionHours;


    QStringList arguments;

    arguments
        << scriptPath
        << QStringLiteral("--station")
        << QString::number(stationId)
        << QStringLiteral("--hours")
        << QString::number(
               evaluationHours
           )
        << QStringLiteral("--output")
        << outputPath;


    connect(
        m_evaluationProcess,
        &QProcess::finished,
        this,
        [this](
            int exitCode,
            QProcess::ExitStatus exitStatus)
        {
            const QString output =
                QString::fromUtf8(
                    m_evaluationProcess
                        ->readAllStandardOutput()
                );

            const QString error =
                QString::fromUtf8(
                    m_evaluationProcess
                        ->readAllStandardError()
                );


            m_runEvaluationButton->setEnabled(
                true
            );

            m_runEvaluationButton->setText(
                QStringLiteral(
                    "运行模型回测"
                )
            );


            if (exitStatus
                    != QProcess::NormalExit
                || exitCode != 0) {

                QMessageBox::warning(
                    this,
                    QStringLiteral("模型回测失败"),
                    error.isEmpty()
                        ? output
                        : error
                );

            } else {

                loadEvaluationResults();
            }


            m_evaluationProcess->deleteLater();

            m_evaluationProcess = nullptr;
        }
    );


    m_evaluationProcess->start(
        pythonPath,
        arguments
    );
}

void MainWindow::loadEvaluationResults()
{
    if (!m_evaluationChartView
        || !m_evaluationTable) {
        return;
    }


    const QString filePath =
        QDir(
            QStringLiteral(
                NCS_PROJECT_ROOT
            )
        ).filePath(
            QStringLiteral(
                "ml/evaluation_results.csv"
            )
        );


    QFile file(filePath);

    if (!file.open(
            QIODevice::ReadOnly
            | QIODevice::Text)) {

        QMessageBox::warning(
            this,
            QStringLiteral("模型回测"),
            QStringLiteral(
                "无法读取模型回测结果。"
            )
        );

        return;
    }


    QTextStream stream(&file);

    // Skip CSV header.
    if (!stream.atEnd()) {
        stream.readLine();
    }


    auto *actualSeries =
        new QLineSeries();

    auto *predictedSeries =
        new QLineSeries();


    actualSeries->setName(
        QStringLiteral("实际负荷")
    );

    predictedSeries->setName(
        QStringLiteral("预测负荷")
    );


    QPen actualPen(
        QColor(
            QStringLiteral("#60A5FA")
        )
    );

    actualPen.setWidth(3);

    actualSeries->setPen(actualPen);


    QPen predictionPen(
        QColor(
            QStringLiteral("#10B981")
        )
    );

    predictionPen.setWidth(3);

    predictedSeries->setPen(
        predictionPen
    );


    m_evaluationTable->setRowCount(0);


    QDateTime minimumTime;
    QDateTime maximumTime;

    double maximumLoad = 0.0;


    while (!stream.atEnd()) {

        const QString line =
            stream.readLine().trimmed();

        if (line.isEmpty()) {
            continue;
        }

        const QStringList fields =
            line.split(',');

        if (fields.size() < 5) {
            continue;
        }


        const QDateTime time =
            QDateTime::fromString(
                fields.at(0),
                QStringLiteral(
                    "yyyy-MM-dd HH:mm:ss"
                )
            );


        const double actual =
            fields.at(2).toDouble();

        const double predicted =
            fields.at(3).toDouble();

        const double error =
            fields.at(4).toDouble();


        if (!time.isValid()) {
            continue;
        }


        const qint64 milliseconds =
            time.toMSecsSinceEpoch();


        actualSeries->append(
            milliseconds,
            actual
        );

        predictedSeries->append(
            milliseconds,
            predicted
        );


        maximumLoad =
            qMax(
                maximumLoad,
                qMax(
                    actual,
                    predicted
                )
            );


        if (!minimumTime.isValid()
            || time < minimumTime) {
            minimumTime = time;
        }

        if (!maximumTime.isValid()
            || time > maximumTime) {
            maximumTime = time;
        }


        const int row =
            m_evaluationTable->rowCount();

        m_evaluationTable->insertRow(row);


        const QStringList values = {
            time.toString(
                QStringLiteral(
                    "yyyy-MM-dd HH:mm"
                )
            ),

            QString::number(
                actual,
                'f',
                2
            ),

            QString::number(
                predicted,
                'f',
                2
            ),

            QString::number(
                error,
                'f',
                2
            )
        };


        for (int column = 0;
             column < values.size();
             ++column) {

            auto *item =
                new QTableWidgetItem(
                    values.at(column)
                );

            item->setTextAlignment(
                Qt::AlignCenter
            );

            m_evaluationTable->setItem(
                row,
                column,
                item
            );
        }
    }


    file.close();


    // ==========================================
    // Chart
    // ==========================================

    auto *chart = new QChart();

    styleChart(chart);

    chart->addSeries(
        actualSeries
    );

    chart->addSeries(
        predictedSeries
    );


    auto *axisX =
        new QDateTimeAxis(chart);

    axisX->setFormat(
        QStringLiteral(
            "MM-dd HH:mm"
        )
    );

    axisX->setTitleText(
        QStringLiteral("时间")
    );


    if (minimumTime.isValid()
        && maximumTime.isValid()) {

        axisX->setRange(
            minimumTime,
            maximumTime
        );
    }


    auto *axisY =
        new QValueAxis(chart);

    axisY->setTitleText(
        QStringLiteral("负荷(kWh)")
    );

    axisY->setRange(
        0.0,
        qMax(
            1.0,
            maximumLoad * 1.20
        )
    );


    axisX->setLabelsColor(
        QColor("#8FA1AC")
    );

    axisY->setLabelsColor(
        QColor("#8FA1AC")
    );

    axisX->setGridLineColor(
        QColor("#25364A")
    );

    axisY->setGridLineColor(
        QColor("#25364A")
    );


    chart->addAxis(
        axisX,
        Qt::AlignBottom
    );

    chart->addAxis(
        axisY,
        Qt::AlignLeft
    );


    actualSeries->attachAxis(axisX);
    actualSeries->attachAxis(axisY);

    predictedSeries->attachAxis(axisX);
    predictedSeries->attachAxis(axisY);


    chart->legend()->setVisible(true);

    chart->legend()->setLabelColor(
        QColor("#EAF3FF")
    );


    m_evaluationChartView->setChart(
        chart
    );
}
