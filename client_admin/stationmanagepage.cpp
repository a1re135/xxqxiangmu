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
#include <QSplitter>
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

void MainWindow::setupStationManagement()
{
    auto *page = uiObject<QWidget>(this, QStringLiteral("stationManagePage"));
    auto *rootLayout = uiObject<QVBoxLayout>(this, QStringLiteral("stationManagePageLayout"));
    if (!page || !rootLayout) return;

    while (rootLayout->count() > 0) {
        QLayoutItem *item = rootLayout->takeAt(0);
        if (QWidget *widget = item->widget()) widget->deleteLater();
        delete item;
    }

    rootLayout->setContentsMargins(28, 24, 28, 24);
    rootLayout->setSpacing(14);

    auto *header = new QHBoxLayout();
    header->setSpacing(14);
    auto *icon = new QLabel(QStringLiteral("⌂"), page);
    icon->setFixedSize(50, 50);
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet(QStringLiteral(
        "QLabel { color:#10B981; background:#102D2B; border:1px solid #1B5148; border-radius:14px; font-size:26px; font-weight:900; }"));
    auto *titleBox = new QVBoxLayout();
    titleBox->setSpacing(2);
    auto *title = new QLabel(QStringLiteral("充电站管理"), page);
    title->setStyleSheet(QStringLiteral("QLabel { color:#F5F7FA; font-size:25px; font-weight:900; }"));
    auto *subtitle = new QLabel(QStringLiteral("维护电站基础信息、站内电桩与在线率，支持新增、修改与删除"), page);
    subtitle->setStyleSheet(QStringLiteral("QLabel { color:#8FA1AC; font-size:14px; font-weight:600; }"));
    titleBox->addWidget(title); titleBox->addWidget(subtitle);
    header->addWidget(icon); header->addLayout(titleBox); header->addStretch();

    auto *addButton = new QPushButton(QStringLiteral("＋  新增电站"), page);
    auto buttonStyle = QStringLiteral(
        "QPushButton { background:#10B981; color:#071A15; border:none; border-radius:10px; padding:11px 20px; font-size:14px; font-weight:900; }"
        "QPushButton:hover { background:#16D497; }");
    addButton->setStyleSheet(buttonStyle);
    addButton->setMinimumHeight(44);
    header->addWidget(addButton);
    m_addStationButton = addButton;
    rootLayout->addLayout(header);

    auto *stationCard = new QFrame(page);
    stationCard->setStyleSheet(QStringLiteral("QFrame { background:#121D30; border:1px solid #25364A; border-radius:14px; }"));
    auto *stationLayout = new QVBoxLayout(stationCard);
    stationLayout->setContentsMargins(14, 14, 14, 14);
    stationLayout->setSpacing(10);
    auto *tableTitle = new QLabel(QStringLiteral("电站列表"), stationCard);
    tableTitle->setStyleSheet(QStringLiteral("QLabel { color:#F3F6F8; font-size:19px; font-weight:850; }"));
    auto *tableHint = new QLabel(QStringLiteral("点击电站行，下方查看该站全部电桩实时状态"), stationCard);
    tableHint->setStyleSheet(QStringLiteral("QLabel { color:#718394; font-size:12px; font-weight:600; }"));
    stationLayout->addWidget(tableTitle); stationLayout->addWidget(tableHint);

    m_stationTable = new QTableWidget(0, 8, stationCard);
    m_stationTable->setHorizontalHeaderLabels({QStringLiteral("电站ID"), QStringLiteral("站名"), QStringLiteral("详细地址"), QStringLiteral("经度"), QStringLiteral("纬度"), QStringLiteral("单价(元/度)"), QStringLiteral("总电桩数"), QStringLiteral("在线率")});
    m_stationTable->verticalHeader()->setVisible(false);
    m_stationTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_stationTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_stationTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_stationTable->setShowGrid(false);
    m_stationTable->setAlternatingRowColors(false);
    // Keep the table compact enough for the admin window's 1280x800 minimum size.
    // The vertical splitter below lets both the station table and charger details
    // share the available height without overlap.
    m_stationTable->setMinimumHeight(110);
    m_stationTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_stationTable->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_stationTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_stationTable->verticalHeader()->setDefaultSectionSize(34);
    m_stationTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_stationTable->horizontalHeader()->setStretchLastSection(false);
    m_stationTable->setStyleSheet(QStringLiteral(
        "QTableWidget { background:#101A2B; color:#EAF0F3; border:1px solid #27394D; border-radius:10px; font-size:14px; font-weight:600; selection-background-color:#153B35; selection-color:#F4F7F8; }"
        "QHeaderView::section { background:#1A2739; color:#8FA1AC; border:none; padding:11px 8px; font-size:13px; font-weight:800; }"
        "QTableWidget::item { padding:9px 8px; border-bottom:1px solid #1D2B3C; }"));
    const int widths[] = {66, 140, 240, 86, 86, 104, 88, 88};
    for (int i=0;i<8;++i) m_stationTable->setColumnWidth(i,widths[i]);
    m_stationTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_stationTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_stationTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_stationTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_stationTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_stationTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    m_stationTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
    m_stationTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Fixed);
    stationLayout->addWidget(m_stationTable, 1);

    auto *detailCard = new QFrame(page);
    detailCard->setStyleSheet(QStringLiteral("QFrame { background:#121D30; border:1px solid #25364A; border-radius:14px; }"));
    auto *detailLayout = new QVBoxLayout(detailCard);
    detailLayout->setContentsMargins(14, 14, 14, 14);
    detailLayout->setSpacing(10);
    auto *detailHead = new QHBoxLayout();
    auto *detailTitle = new QLabel(QStringLiteral("站内电桩实时状态"), detailCard);
    detailTitle->setStyleSheet(QStringLiteral("QLabel { color:#F3F6F8; font-size:19px; font-weight:850; }"));
    m_stationSummaryLabel = new QLabel(QStringLiteral("未选择电站"), detailCard);
    m_stationSummaryLabel->setStyleSheet(QStringLiteral("QLabel { color:#8FA1AC; font-size:13px; font-weight:700; }"));
    detailHead->addWidget(detailTitle); detailHead->addStretch(); detailHead->addWidget(m_stationSummaryLabel);
    detailLayout->addLayout(detailHead);

    m_stationChargerTable = new QTableWidget(0, 6, detailCard);
    m_stationChargerTable->setHorizontalHeaderLabels({QStringLiteral("电桩编号"), QStringLiteral("类型"), QStringLiteral("功率(kW)"), QStringLiteral("状态"), QStringLiteral("累计次数"), QStringLiteral("累计时长(小时)")});
    m_stationChargerTable->verticalHeader()->setVisible(false);
    m_stationChargerTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_stationChargerTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_stationChargerTable->setShowGrid(false);
    m_stationChargerTable->horizontalHeader()->setStretchLastSection(true);
    m_stationChargerTable->setMinimumHeight(80);
    m_stationChargerTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_stationChargerTable->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_stationChargerTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_stationChargerTable->verticalHeader()->setDefaultSectionSize(32);
    m_stationChargerTable->setStyleSheet(QStringLiteral(
        "QTableWidget { background:#101A2B; color:#EAF0F3; border:1px solid #27394D; border-radius:10px; font-size:13px; font-weight:600; }"
        "QHeaderView::section { background:#1A2739; color:#8FA1AC; border:none; padding:10px 8px; font-size:12px; font-weight:800; }"
        "QTableWidget::item { padding:8px; border-bottom:1px solid #1D2B3C; }"));
    // Let the "累计时长(小时)" column absorb the remaining width so the
    // detail table fills the entire card instead of leaving a large blank area
    // to the right when the admin window is wide.
    m_stationChargerTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    const int chargerWidths[] = {150, 90, 100, 100, 100, 150};
    for (int i=0; i<6; ++i) {
        m_stationChargerTable->setColumnWidth(i, chargerWidths[i]);
    }
    m_stationChargerTable->horizontalHeader()->setSectionResizeMode(
        5, QHeaderView::Stretch);
    detailLayout->addWidget(m_stationChargerTable, 1);

    auto *actions = new QHBoxLayout(); actions->setSpacing(10);
    auto makeAction = [page](const QString &text) {
        auto *b = new QPushButton(text, page);
        b->setMinimumHeight(42);
        b->setStyleSheet(QStringLiteral(
            "QPushButton { background:#232D3B; color:#E8EEF1; border:1px solid #364759; border-radius:9px; padding:0 18px; font-size:14px; font-weight:850; }"
            "QPushButton:hover { background:#2C3949; } QPushButton:disabled { color:#59656F; background:#1A222C; border-color:#27313D; }"));
        return b;
    };
    m_editStationButton = makeAction(QStringLiteral("编辑电站"));
    m_deleteStationButton = makeAction(QStringLiteral("删除电站"));
    m_editStationButton->setEnabled(false); m_deleteStationButton->setEnabled(false);
    auto *hint = new QLabel(QStringLiteral("BR-10：电站仍有电桩时禁止删除"), page);
    hint->setStyleSheet(QStringLiteral("QLabel { color:#6F8190; font-size:12px; font-weight:650; }"));
    actions->addWidget(m_editStationButton); actions->addWidget(m_deleteStationButton); actions->addStretch(); actions->addWidget(hint);
    detailLayout->addLayout(actions);

    // Use a vertical splitter so the two management areas never overlap at the
    // minimum 1280x800 admin size. Users can also drag the divider when needed.
    auto *splitter = new QSplitter(Qt::Vertical, page);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(6);
    splitter->setStyleSheet(QStringLiteral(
        "QSplitter::handle { background:#1B2C3D; border-radius:3px; margin:2px 80px; }"
        "QSplitter::handle:hover { background:#10B981; }"));
    stationCard->setMinimumHeight(190);
    detailCard->setMinimumHeight(190);
    splitter->addWidget(stationCard);
    splitter->addWidget(detailCard);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes(QList<int>() << 300 << 300);

    rootLayout->addWidget(splitter, 1);

    connect(addButton, &QPushButton::clicked, this, &MainWindow::addNewStation);
    connect(m_editStationButton, &QPushButton::clicked, this, &MainWindow::editSelectedStation);
    connect(m_deleteStationButton, &QPushButton::clicked, this, &MainWindow::deleteSelectedStation);
    connect(m_stationTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::stationSelectionChanged);
}


void MainWindow::refreshStationManagement(int preferredStationId)
{
    if (!m_stationTable || !m_stationChargerTable) return;

    QList<StationService::StationRecord> stations;
    QString errorMessage;
    if (!m_stationService.loadStations(stations, errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("充电站管理"), errorMessage);
        return;
    }
    m_stationRecords = stations;

    m_stationTable->setRowCount(0);
    int targetRow = -1;
    for (int i=0; i<stations.size(); ++i) {
        const auto &st = stations.at(i);
        const int row = m_stationTable->rowCount();
        m_stationTable->insertRow(row);
        auto add = [this,row](int col, const QString &text) {
            auto *item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignCenter);
            m_stationTable->setItem(row,col,item);
        };
        add(0, QString::number(st.id));
        add(1, st.name);
        add(2, st.address);
        add(3, QString::number(st.longitude, 'f', 4));
        add(4, QString::number(st.latitude, 'f', 4));
        add(5, QString::number(st.price, 'f', 2));
        add(6, QString::number(st.totalChargers));
        add(7, QStringLiteral("%1%").arg(st.onlineRate, 0, 'f', 2));
        m_stationTable->item(row,7)->setForeground(QBrush(QColor(st.onlineRate >= 90.0 ? QStringLiteral("#10B981") : (st.onlineRate >= 60.0 ? QStringLiteral("#F6A648") : QStringLiteral("#F05252")))));
        if (st.id == preferredStationId) targetRow = row;
    }

    if (m_stationTable->rowCount() == 0) {
        m_stationChargerTable->setRowCount(0);
        m_stationSummaryLabel->setText(QStringLiteral("暂无电站数据"));
        m_editStationButton->setEnabled(false); m_deleteStationButton->setEnabled(false);
        return;
    }

    targetRow = targetRow >= 0 ? targetRow : 0;
    m_stationTable->selectRow(targetRow);
    stationSelectionChanged();
}


void MainWindow::stationSelectionChanged()
{
    if (!m_stationTable || !m_stationChargerTable) return;
    const int row = m_stationTable->currentRow();
    if (row < 0 || row >= m_stationRecords.size()) {
        m_selectedStationId = 0;
        m_editStationButton->setEnabled(false); m_deleteStationButton->setEnabled(false);
        m_stationSummaryLabel->setText(QStringLiteral("未选择电站"));
        m_stationChargerTable->setRowCount(0);
        return;
    }

    const auto station = m_stationRecords.at(row);
    m_selectedStationId = station.id;
    m_editStationButton->setEnabled(true); m_deleteStationButton->setEnabled(true);
    m_stationSummaryLabel->setText(QStringLiteral("%1 · %2 个电桩").arg(station.name).arg(station.totalChargers));

    QList<StationService::ChargerDetail> chargers;
    QString errorMessage;
    if (!m_stationService.loadChargersForStation(station.id, chargers, errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("站内电桩"), errorMessage);
        return;
    }

    m_stationChargerTable->setRowCount(0);
    for (const auto &c : chargers) {
        const int r = m_stationChargerTable->rowCount();
        m_stationChargerTable->insertRow(r);
        QStringList values = {
            c.chargerNo,
            c.type == 1 ? QStringLiteral("快充") : QStringLiteral("慢充"),
            QString::number(c.power, 'f', 1),
            c.status == 1 ? QStringLiteral("在用") : (c.status == 2 ? QStringLiteral("故障") : QStringLiteral("闲置")),
            QString::number(c.totalCount),
            QString::number(static_cast<double>(c.totalMinutes) / 60.0, 'f', 2)
        };
        for (int col=0; col<values.size(); ++col) {
            auto *item = new QTableWidgetItem(values.at(col));
            item->setTextAlignment(Qt::AlignCenter);
            if (col == 3) {
                item->setForeground(QBrush(QColor(c.status == 1 ? QStringLiteral("#F6A648") : (c.status == 2 ? QStringLiteral("#F05252") : QStringLiteral("#10B981")))));
            }
            m_stationChargerTable->setItem(r,col,item);
        }
    }
    if (chargers.isEmpty()) {
        m_stationChargerTable->setRowCount(1);
        auto *empty = new QTableWidgetItem(QStringLiteral("该电站暂无电桩"));
        empty->setTextAlignment(Qt::AlignCenter);
        empty->setForeground(QBrush(QColor(QStringLiteral("#7E909B"))));
        m_stationChargerTable->setItem(0,0,empty);
        m_stationChargerTable->setSpan(0,0,1,6);
    }
}


void MainWindow::addNewStation()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("新增充电站"));
    dialog.resize(540, 520);
    dialog.setModal(true);
    dialog.setStyleSheet(QStringLiteral(
        "QDialog { background:#0F1929; color:#EDF4F6; }"
        "QLabel { color:#AAB8C0; font-size:14px; font-weight:700; }"
        "QLineEdit, QDoubleSpinBox, QSpinBox { background:#182437; color:#EEF4F6; border:1px solid #314257; border-radius:8px; padding:8px 10px; min-height:20px; font-size:14px; }"
        "QLineEdit:focus, QDoubleSpinBox:focus, QSpinBox:focus { border:1px solid #10B981; }"
        "QPushButton { background:#253244; color:#E6EDF0; border:1px solid #36475A; border-radius:8px; padding:8px 18px; font-size:14px; font-weight:800; }"
        "QPushButton:hover { background:#2D3C4F; }"));
    auto *layout = new QVBoxLayout(&dialog); layout->setContentsMargins(24,22,24,22); layout->setSpacing(13);
    auto *heading = new QLabel(QStringLiteral("新增充电站"), &dialog); heading->setStyleSheet(QStringLiteral("QLabel { color:#F5F7FA; font-size:22px; font-weight:900; }")); layout->addWidget(heading);
    auto *form = new QFormLayout(); form->setSpacing(12);
    auto *name = new QLineEdit(&dialog);
    auto *address = new QLineEdit(&dialog);
    auto *longitude = new QDoubleSpinBox(&dialog); longitude->setRange(-180,180); longitude->setDecimals(6); longitude->setSingleStep(0.0001);
    auto *latitude = new QDoubleSpinBox(&dialog); latitude->setRange(-90,90); latitude->setDecimals(6); latitude->setSingleStep(0.0001);
    auto *price = new QDoubleSpinBox(&dialog); price->setRange(0,9999); price->setDecimals(2); price->setValue(1.50); price->setSuffix(QStringLiteral(" 元/度"));
    auto *count = new QSpinBox(&dialog); count->setRange(0,999); count->setValue(6); count->setSuffix(QStringLiteral(" 台"));
    auto *power = new QDoubleSpinBox(&dialog); power->setRange(0.1,1000); power->setDecimals(1); power->setValue(7.0); power->setSuffix(QStringLiteral(" kW"));
    name->setPlaceholderText(QStringLiteral("例如 丰台充电站")); address->setPlaceholderText(QStringLiteral("详细地址"));
    form->addRow(QStringLiteral("站名"), name); form->addRow(QStringLiteral("详细地址"), address); form->addRow(QStringLiteral("经度"), longitude); form->addRow(QStringLiteral("纬度"), latitude); form->addRow(QStringLiteral("单价"), price); form->addRow(QStringLiteral("初始电桩数量"), count); form->addRow(QStringLiteral("默认功率"), power); layout->addLayout(form);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确认新增")); buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消")); layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (name->text().trimmed().isEmpty() || address->text().trimmed().isEmpty()) { QMessageBox::warning(&dialog, QStringLiteral("新增电站"), QStringLiteral("站名和详细地址不能为空。")); return; }
        QString err;
        if (!m_stationService.addStation(name->text(), address->text(), longitude->value(), latitude->value(), price->value(), count->value(), power->value(), err)) { QMessageBox::warning(&dialog, QStringLiteral("新增电站"), err); return; }
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) refreshStationManagement(-1);
}


void MainWindow::editSelectedStation()
{
    if (m_selectedStationId <= 0) return;
    auto it = std::find_if(m_stationRecords.cbegin(), m_stationRecords.cend(), [this](const auto &s){ return s.id == m_selectedStationId; });
    if (it == m_stationRecords.cend()) return;
    const auto original = *it;

    QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("修改充电站")); dialog.resize(520,430); dialog.setModal(true);
    dialog.setStyleSheet(QStringLiteral("QDialog { background:#0F1929; color:#EDF4F6; } QLabel { color:#AAB8C0; font-size:14px; font-weight:700; } QLineEdit, QDoubleSpinBox { background:#182437; color:#EEF4F6; border:1px solid #314257; border-radius:8px; padding:8px 10px; min-height:20px; font-size:14px; } QPushButton { background:#253244; color:#E6EDF0; border:1px solid #36475A; border-radius:8px; padding:8px 18px; font-size:14px; font-weight:800; }"));
    auto *layout = new QVBoxLayout(&dialog); layout->setContentsMargins(24,22,24,22); layout->setSpacing(13);
    auto *heading = new QLabel(QStringLiteral("修改充电站"), &dialog); heading->setStyleSheet(QStringLiteral("QLabel { color:#F5F7FA; font-size:22px; font-weight:900; }")); layout->addWidget(heading);
    auto *form = new QFormLayout(); form->setSpacing(12);
    auto *name = new QLineEdit(original.name, &dialog); auto *address = new QLineEdit(original.address, &dialog);
    auto *longitude = new QDoubleSpinBox(&dialog); longitude->setRange(-180,180); longitude->setDecimals(6); longitude->setValue(original.longitude);
    auto *latitude = new QDoubleSpinBox(&dialog); latitude->setRange(-90,90); latitude->setDecimals(6); latitude->setValue(original.latitude);
    auto *price = new QDoubleSpinBox(&dialog); price->setRange(0,9999); price->setDecimals(2); price->setValue(original.price);
    form->addRow(QStringLiteral("站名"), name); form->addRow(QStringLiteral("详细地址"), address); form->addRow(QStringLiteral("经度"), longitude); form->addRow(QStringLiteral("纬度"), latitude); form->addRow(QStringLiteral("单价"), price); layout->addLayout(form);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog); buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("保存修改")); buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消")); layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        QString err; if (!m_stationService.updateStation(original.id, name->text(), address->text(), longitude->value(), latitude->value(), price->value(), err)) { QMessageBox::warning(&dialog, QStringLiteral("修改电站"), err); return; } dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) refreshStationManagement(original.id);
}


void MainWindow::deleteSelectedStation()
{
    if (m_selectedStationId <= 0) return;
    auto it = std::find_if(m_stationRecords.cbegin(), m_stationRecords.cend(), [this](const auto &s){ return s.id == m_selectedStationId; });
    if (it == m_stationRecords.cend()) return;
    const auto station = *it;
    if (station.totalChargers > 0) {
        QMessageBox::warning(this, QStringLiteral("删除电站"), QStringLiteral("该电站下仍有 %1 个电桩，禁止删除。\n请先删除站内电桩后再删除电站。BR-10 规则已生效。").arg(station.totalChargers));
        return;
    }
    const auto result = QMessageBox::warning(this, QStringLiteral("确认删除"), QStringLiteral("确定删除电站「%1」吗？此操作不可撤销。").arg(station.name), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (result != QMessageBox::Yes) return;
    QString err; if (!m_stationService.deleteStation(station.id, err)) { QMessageBox::warning(this, QStringLiteral("删除电站"), err); return; }
    m_selectedStationId = 0; refreshStationManagement(-1);
}


