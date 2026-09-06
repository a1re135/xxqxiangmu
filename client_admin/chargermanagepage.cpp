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

void MainWindow::setupChargerManagement()
{
    auto *page = uiObject<QWidget>(this, QStringLiteral("chargerManagePage"));
    auto *rootLayout = uiObject<QVBoxLayout>(this, QStringLiteral("chargerManagePageLayout"));
    if (!page || !rootLayout) {
        return;
    }

    // Remove the designer placeholder content and reuse its existing layout.
    while (rootLayout->count() > 0) {
        QLayoutItem *item = rootLayout->takeAt(0);
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }

    rootLayout->setContentsMargins(28, 24, 28, 24);
    rootLayout->setSpacing(16);

    auto *header = new QHBoxLayout();
    header->setSpacing(14);

    auto *icon = new QLabel(QStringLiteral("▤"), page);
    icon->setFixedSize(48, 48);
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet(QStringLiteral(
        "QLabel { color:#10B981; background:#102D2B; border:1px solid #1B5148; "
        "border-radius:14px; font-size:24px; font-weight:900; }"));

    auto *titleBox = new QVBoxLayout();
    titleBox->setSpacing(2);
    auto *title = new QLabel(QStringLiteral("充电桩管理"), page);
    title->setStyleSheet(QStringLiteral(
        "QLabel { color:#F5F7FA; font-size:24px; font-weight:900; }"));
    auto *subtitle = new QLabel(
        QStringLiteral("查看全部电桩运行状态，支持筛选、搜索与远程运维"),
        page);
    subtitle->setStyleSheet(QStringLiteral(
        "QLabel { color:#8FA1AC; font-size:14px; font-weight:600; }"));
    titleBox->addWidget(title);
    titleBox->addWidget(subtitle);

    header->addWidget(icon);
    header->addLayout(titleBox);
    header->addStretch();

    auto *hint = new QLabel(QStringLiteral("数据实时读取自 charger 表"), page);
    hint->setStyleSheet(QStringLiteral(
        "QLabel { color:#718394; background:#162336; border:1px solid #2B3A4D; "
        "border-radius:10px; padding:9px 12px; font-size:13px; font-weight:700; }"));
    header->addWidget(hint);

    rootLayout->addLayout(header);

    // Filters.
    auto *filterCard = new QFrame(page);
    filterCard->setStyleSheet(QStringLiteral(
        "QFrame { background:#121D30; border:1px solid #25364A; border-radius:14px; }"));
    auto *filterLayout = new QHBoxLayout(filterCard);
    filterLayout->setContentsMargins(16, 12, 16, 12);
    filterLayout->setSpacing(10);

    auto *stationLabel = new QLabel(QStringLiteral("电站"), filterCard);
    stationLabel->setStyleSheet(QStringLiteral("QLabel { color:#93A4AE; font-size:14px; font-weight:800; }"));
    m_stationFilter = new QComboBox(filterCard);
    m_stationFilter->setObjectName(QStringLiteral("chargerStationFilter"));
    m_stationFilter->setMinimumHeight(40);
    m_stationFilter->setMinimumWidth(180);

    auto *statusLabel = new QLabel(QStringLiteral("状态"), filterCard);
    statusLabel->setStyleSheet(QStringLiteral("QLabel { color:#93A4AE; font-size:14px; font-weight:800; }"));
    m_statusFilter = new QComboBox(filterCard);
    m_statusFilter->setObjectName(QStringLiteral("chargerStatusFilter"));
    m_statusFilter->setMinimumHeight(40);
    m_statusFilter->setMinimumWidth(140);

    auto *searchLabel = new QLabel(QStringLiteral("编号"), filterCard);
    searchLabel->setStyleSheet(QStringLiteral("QLabel { color:#93A4AE; font-size:14px; font-weight:800; }"));
    m_chargerSearch = new QLineEdit(filterCard);
    m_chargerSearch->setObjectName(QStringLiteral("chargerSearch"));
    m_chargerSearch->setPlaceholderText(QStringLiteral("搜索电桩编号"));
    m_chargerSearch->setClearButtonEnabled(true);
    m_chargerSearch->setMinimumHeight(40);
    m_chargerSearch->setMinimumWidth(220);

    filterLayout->addWidget(stationLabel);
    filterLayout->addWidget(m_stationFilter);
    filterLayout->addSpacing(8);
    filterLayout->addWidget(statusLabel);
    filterLayout->addWidget(m_statusFilter);
    filterLayout->addSpacing(8);
    filterLayout->addWidget(searchLabel);
    filterLayout->addWidget(m_chargerSearch);
    filterLayout->addStretch();

    rootLayout->addWidget(filterCard);

    // Table.
    auto *tableCard = new QFrame(page);
    tableCard->setStyleSheet(QStringLiteral(
        "QFrame { background:#121D30; border:1px solid #25364A; border-radius:14px; }"));
    auto *tableLayout = new QVBoxLayout(tableCard);
    tableLayout->setContentsMargins(14, 14, 14, 14);

    m_chargerTable = new QTableView(tableCard);
    m_chargerTable->setObjectName(QStringLiteral("chargerManagementTable"));
    m_chargerTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_chargerTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_chargerTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_chargerTable->setAlternatingRowColors(false);
    m_chargerTable->setShowGrid(false);
    m_chargerTable->setSortingEnabled(false);
    m_chargerTable->verticalHeader()->setVisible(false);
    m_chargerTable->horizontalHeader()->setStretchLastSection(true);
    m_chargerTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_chargerTable->setMinimumHeight(330);
    m_chargerTable->setStyleSheet(QStringLiteral(
        "QTableView { background:#101A2B; color:#EAF0F3; border:1px solid #27394D; "
        "border-radius:10px; font-size:14px; font-weight:600; selection-background-color:#153B35; "
        "selection-color:#F4F7F8; }"
        "QHeaderView::section { background:#1A2739; color:#8FA1AC; border:none; "
        "padding:12px 8px; font-size:13px; font-weight:800; }"
        "QTableView::item { padding:10px 8px; border-bottom:1px solid #1D2B3C; }"));

    m_chargerModel = new QStandardItemModel(0, 7, m_chargerTable);
    m_chargerModel->setHorizontalHeaderLabels({
        QStringLiteral("电桩编号"),
        QStringLiteral("所属电站"),
        QStringLiteral("类型"),
        QStringLiteral("功率(kW)"),
        QStringLiteral("当前状态"),
        QStringLiteral("累计充电次数"),
        QStringLiteral("累计充电时长(小时)")
    });
    m_chargerTable->setModel(m_chargerModel);

    const int widths[] = {120, 220, 90, 105, 110, 130, 160};
    for (int i = 0; i < 7; ++i) {
        m_chargerTable->setColumnWidth(i, widths[i]);
    }

    tableLayout->addWidget(m_chargerTable);
    rootLayout->addWidget(tableCard, 1);

    // Action bar.
    auto *actionCard = new QFrame(page);
    actionCard->setStyleSheet(QStringLiteral(
        "QFrame { background:#121D30; border:1px solid #25364A; border-radius:14px; }"));
    auto *actionLayout = new QHBoxLayout(actionCard);
    actionLayout->setContentsMargins(14, 12, 14, 12);
    actionLayout->setSpacing(10);

    auto makeButton = [actionCard](const QString &text, const QString &style) {
        auto *button = new QPushButton(text, actionCard);
        button->setMinimumHeight(42);
        button->setStyleSheet(style);
        return button;
    };

    m_restartChargerButton = makeButton(
        QStringLiteral("远程重启"),
        QStringLiteral(
            "QPushButton { background:#163B38; color:#63E6BE; border:1px solid #27665B; "
            "border-radius:9px; padding:0 18px; font-size:14px; font-weight:850; }"
            "QPushButton:hover { background:#1B4B45; }"
            "QPushButton:disabled { color:#566B70; background:#17232C; border-color:#26343C; }"));
    m_faultChargerButton = makeButton(
        QStringLiteral("标记故障"),
        QStringLiteral(
            "QPushButton { background:#3A2426; color:#F69A9A; border:1px solid #6A3A3A; "
            "border-radius:9px; padding:0 18px; font-size:14px; font-weight:850; }"
            "QPushButton:hover { background:#4A2A2D; }"
            "QPushButton:disabled { color:#655456; background:#202124; border-color:#383437; }"));
    m_recoverChargerButton = makeButton(
        QStringLiteral("恢复正常"),
        QStringLiteral(
            "QPushButton { background:#123C30; color:#68E0B2; border:1px solid #276B55; "
            "border-radius:9px; padding:0 18px; font-size:14px; font-weight:850; }"
            "QPushButton:hover { background:#174D3D; }"
            "QPushButton:disabled { color:#566B70; background:#17232C; border-color:#26343C; }"));
    m_addChargerButton = makeButton(
        QStringLiteral("新增电桩"),
        QStringLiteral(
            "QPushButton { background:#10B981; color:#071A15; border:none; border-radius:9px; "
            "padding:0 20px; font-size:14px; font-weight:900; }"
            "QPushButton:hover { background:#16D497; }"));
    m_deleteChargerButton = makeButton(
        QStringLiteral("删除电桩"),
        QStringLiteral(
            "QPushButton { background:#232B38; color:#DDE5E9; border:1px solid #344354; "
            "border-radius:9px; padding:0 18px; font-size:14px; font-weight:850; }"
            "QPushButton:hover { background:#2C3745; }"
            "QPushButton:disabled { color:#59656F; background:#1A222C; border-color:#27313D; }"));

    actionLayout->addWidget(m_restartChargerButton);
    actionLayout->addWidget(m_faultChargerButton);
    actionLayout->addWidget(m_recoverChargerButton);
    actionLayout->addSpacing(8);
    actionLayout->addWidget(m_addChargerButton);
    actionLayout->addWidget(m_deleteChargerButton);
    actionLayout->addStretch();

    auto *help = new QLabel(QStringLiteral("选择一行后执行对应操作"), actionCard);
    help->setStyleSheet(QStringLiteral(
        "QLabel { color:#6F8190; font-size:12px; font-weight:650; }"));
    actionLayout->addWidget(help);

    rootLayout->addWidget(actionCard);

    m_stationFilter->addItem(QStringLiteral("全部电站"), -1);
    m_statusFilter->addItem(QStringLiteral("全部状态"), -1);
    m_statusFilter->addItem(QStringLiteral("闲置"), 0);
    m_statusFilter->addItem(QStringLiteral("在用"), 1);
    m_statusFilter->addItem(QStringLiteral("故障"), 2);

    connect(m_stationFilter, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::applyChargerFilters);
    connect(m_statusFilter, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::applyChargerFilters);
    connect(m_chargerSearch, &QLineEdit::textChanged,
            this, &MainWindow::applyChargerFilters);

    connect(m_chargerTable->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this](const QItemSelection &, const QItemSelection &) {
        const int status = selectedChargerStatus();
        const bool hasSelection = selectedChargerId() > 0;
        m_restartChargerButton->setEnabled(hasSelection && m_restartProgress == nullptr);
        m_deleteChargerButton->setEnabled(hasSelection && status != 1);
        m_faultChargerButton->setEnabled(hasSelection && status != 2);
        m_recoverChargerButton->setEnabled(hasSelection && status == 2);
    });

    connect(m_restartChargerButton, &QPushButton::clicked,
            this, &MainWindow::restartSelectedCharger);
    connect(m_faultChargerButton, &QPushButton::clicked,
            this, &MainWindow::setSelectedChargerFault);
    connect(m_recoverChargerButton, &QPushButton::clicked,
            this, &MainWindow::recoverSelectedCharger);
    connect(m_addChargerButton, &QPushButton::clicked,
            this, &MainWindow::addNewCharger);
    connect(m_deleteChargerButton, &QPushButton::clicked,
            this, &MainWindow::deleteSelectedCharger);
}


void MainWindow::refreshChargerManagement()
{
    if (!m_chargerModel) {
        return;
    }

    QList<ChargerService::ChargerRecord> records;
    QString errorMessage;
    if (!m_chargerService.loadChargers(records, errorMessage)) {
        m_chargerModel->removeRows(0, m_chargerModel->rowCount());
        QMessageBox::warning(this, QStringLiteral("电桩管理"),
                             errorMessage);
        return;
    }

    m_chargerRecords = records;

    {
        QSignalBlocker blocker(m_stationFilter);
        const int currentStationId = m_stationFilter->currentData().toInt();

        m_stationFilter->clear();
        m_stationFilter->addItem(QStringLiteral("全部电站"), -1);

        QList<ChargerService::StationOption> stations;
        QString stationError;
        if (m_chargerService.loadStations(stations, stationError)) {
            for (const auto &station : stations) {
                m_stationFilter->addItem(station.name, station.id);
            }
        }

        const int index = m_stationFilter->findData(currentStationId);
        m_stationFilter->setCurrentIndex(index >= 0 ? index : 0);
    }

    applyChargerFilters();
}


