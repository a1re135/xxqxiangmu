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
        QStringLiteral("MM-dd")
    );

    axisX->setTitleText(
        QStringLiteral("日期")
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


    m_prediction7Button =
        new QPushButton(
            QStringLiteral("未来 7 天"),
            controlCard
        );

    m_prediction30Button =
        new QPushButton(
            QStringLiteral("未来 30 天"),
            controlCard
        );

    m_prediction7Button->setCheckable(true);
    m_prediction30Button->setCheckable(true);

    m_prediction7Button
        ->setAutoExclusive(true);

    m_prediction30Button
        ->setAutoExclusive(true);

    m_prediction7Button
        ->setChecked(true);

    m_prediction7Button
        ->setStyleSheet(periodStyle);

    m_prediction30Button
        ->setStyleSheet(periodStyle);


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

    controlLayout->addWidget(
        m_prediction7Button
    );

    controlLayout->addWidget(
        m_prediction30Button
    );

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
            QStringLiteral("日均负荷"),
            m_predictionAverageLabel
        ),
        0, 1
    );

    metrics->addWidget(
        makeMetricCard(
            QStringLiteral("预计高峰日期"),
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

    rootLayout->addWidget(
        splitter,
        1
    );


    // ==================================================
    // Signals
    // ==================================================

    connect(
        m_predictionStationCombo,
        &QComboBox::currentIndexChanged,
        this,
        [this](int) {
            refreshPredictionPage();
        }
    );

    connect(
        m_prediction7Button,
        &QPushButton::clicked,
        this,
        [this]() {
            m_predictionDays = 7;
            refreshPredictionPage();
        }
    );

    connect(
        m_prediction30Button,
        &QPushButton::clicked,
        this,
        [this]() {
            m_predictionDays = 30;
            refreshPredictionPage();
        }
    );

    connect(
        m_runPredictionButton,
        &QPushButton::clicked,
        this,
        &MainWindow::runPrediction
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
                m_predictionDays,
                summary,
                predictionError
            );


    // ==================================================
    // 6. Prediction failed
    // ==================================================

    if (!loaded
        || summary.points.size() < m_predictionDays) {

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
        QStringLiteral("%1 kWh/天")
            .arg(
                summary.averageDailyLoad,
                0,
                'f',
                2
            )
    );


    if (summary.peakTime.isValid()) {

        m_predictionPeakLabel->setText(
            QStringLiteral("%1 · %2 kWh")
                .arg(
                    summary.peakTime
                        .date()
                        .toString(
                            QStringLiteral(
                                "MM-dd"
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

    } else {

        m_predictionPeakLabel->setText(
            QStringLiteral("--")
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
                .date()
                .toString(
                    QStringLiteral(
                        "yyyy-MM-dd"
                    )
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
                    ? QStringLiteral("高峰")
                    : QStringLiteral("正常")
            );


        peakItem->setForeground(
            QBrush(
                QColor(
                    point.isPeak
                        ? QStringLiteral(
                            "#F6A648"
                          )
                        : QStringLiteral(
                            "#10B981"
                          )
                )
            )
        );
    }
}


void MainWindow::runPrediction()
{
    if (!m_predictionStationCombo) {
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
            QStringLiteral(
                "请先选择一个充电站。"
            )
        );

        return;
    }


    m_runPredictionButton
        ->setEnabled(false);

    m_runPredictionButton
        ->setText(
            QStringLiteral(
                "正在运行预测..."
            )
        );


    PredictionService::PredictionSummary
        summary;

    QString errorMessage;


    const bool success =
        m_predictionService
            .generatePrediction(
                stationId,
                m_predictionDays,
                summary,
                errorMessage
            );


    m_runPredictionButton
        ->setEnabled(true);

    m_runPredictionButton
        ->setText(
            QStringLiteral(
                "↻ 重新运行预测"
            )
        );


    if (!success) {

        QMessageBox::warning(
            this,
            QStringLiteral(
                "预测失败"
            ),
            errorMessage
        );

        return;
    }


    // Read and display the newly saved prediction
    refreshPredictionPage();


    QMessageBox::information(
        this,
        QStringLiteral(
            "预测完成"
        ),
        QStringLiteral(
            "已完成「%1」未来 %2 天的"
            "充电负荷预测。"
        )
            .arg(
                m_predictionStationCombo
                    ->currentText()
            )
            .arg(
                m_predictionDays
            )
    );
}
