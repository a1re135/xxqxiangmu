#include "mainwindow.h"

#include <QApplication>
#include "ordersettlementdialog.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QAbstractItemView>
#include <QColor>
#include <QHeaderView>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QUrlQuery>
#include <cmath>
#include "navigationdialog.h"
#include <QMessageBox>
#include "ui/station_list_page.h"
#include "service/charge_service.h"


namespace {
constexpr int kWindowWidth = 420;
constexpr int kWindowHeight = 760;
} // namespace

MainWindow::MainWindow(core::StationService *service,
                       const QString &initialRegion, QWidget *parent)
    : QMainWindow(parent)
    , m_service(service)
{
    setupStyle();
    setupUi();
    connect(
        this,
        &MainWindow::chargingRequested,
        this,
        [this](int stationId, int chargerId)
        {
            QWidget *messageParent =
                QApplication::activeModalWidget();

            if (!messageParent) {
                messageParent = this;
            }

            const auto answer = QMessageBox::question(
                messageParent,
                QStringLiteral("预约电桩"),
                QStringLiteral(
                    "确定预约所选电桩吗？\n"
                    "预约成功后会占用该电桩，"
                    "点击“开始充电”后才开始计费。"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No
            );

            if (answer != QMessageBox::Yes) {
                return;
            }

            core::ChargeService chargeService;
            int orderId = -1;
            QString errorMessage;

            const bool success = chargeService.reserveCharger(
                m_currentUser.id,
                stationId,
                chargerId,
                orderId,
                errorMessage
            );

            // 成功或失败都刷新，避免显示过期的空闲数量。
            m_stationPage->refresh();

            if (!success) {
                QMessageBox::warning(
                    messageParent,
                    QStringLiteral("预约失败"),
                    errorMessage
                );
                return;
            }

            QMessageBox::information(
                messageParent,
                QStringLiteral("预约成功"),
                QStringLiteral(
                    "预约订单编号：%1\n"
                    "电桩已保留，尚未开始充电，未扣费。")
                    .arg(orderId)
            );

            // 暂时复用现有订单页查看预约。
            // 下一步在这个页面接入开始充电和倒计时。
            emit settlementRequested(orderId);
        }
    );
    connect(
        this,
        &MainWindow::settlementRequested,
        this,
        [this](int orderId)
        {
            // 选桩提示来自模态详情窗口时，
            // 将结算页放在当前详情窗口上面。
            QWidget *pageParent =
                QApplication::activeModalWidget();

            if (!pageParent) {
                pageParent = this;
            }

            OrderSettlementDialog settlementDialog(
                orderId,
                m_currentUser.id,
                pageParent
            );

            settlementDialog.exec();
        }
    );

    if (!initialRegion.isEmpty()) {
        m_stationPage->setInitialRegion(initialRegion);
    }
    // 主窗口默认进入电站列表页
    m_stack->setCurrentIndex(0);
    m_navStationBtn->setChecked(true);
}

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("NCS 充电用户端"));
    setFixedSize(kWindowWidth, kWindowHeight);

    m_stationPage = new client_user::StationListPage(m_service, this);
    // 点击列表中的距离，直接打开导航页。
    connect(
        m_stationPage,
        &client_user::StationListPage::navigationRequested,
        this,
        [this](const core::StationListItem &item)
        {
            if (m_service->lastRegionName().isEmpty()) {
                QMessageBox::information(
                    this,
                    QStringLiteral("提示"),
                    QStringLiteral("请先完成定位。")
                );
                return;
            }

            NavigationDialog navigationDialog(
                item,
                m_service->lastLatitude(),
                m_service->lastLongitude(),
                QStringLiteral("首页定位点"),
                this
            );

            navigationDialog.exec();
        }
    );
    connect(m_stationPage, &client_user::StationListPage::stationClicked, this,
            &MainWindow::onStationClicked);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_stationPage);

    // 底部导航
    m_navStationBtn = new QPushButton(QStringLiteral("充电"), this);
    m_navMineBtn = new QPushButton(QStringLiteral("个人主页"), this);
    m_navStationBtn->setObjectName(QStringLiteral("navBtn"));
    m_navMineBtn->setObjectName(QStringLiteral("navBtn"));
    m_navStationBtn->setCheckable(true);
    m_navStationBtn->setCursor(Qt::PointingHandCursor);
    m_navMineBtn->setCursor(Qt::PointingHandCursor);

    connect(
        m_navStationBtn,
        &QPushButton::clicked,
        this,
        [this]()
        {
            if (!checkChargingEntry(this)) {
                return;
            }

            m_stack->setCurrentIndex(0);
            m_navStationBtn->setChecked(true);
            m_stationPage->refresh();
        }
    );
    connect(m_navMineBtn, &QPushButton::clicked,
            this, &MainWindow::personalCenterRequested);

    auto *navBar = new QHBoxLayout;
    navBar->setContentsMargins(16, 10, 16, 14);
    navBar->setSpacing(10);
    navBar->addWidget(m_navStationBtn, 1);
    navBar->addWidget(m_navMineBtn, 1);

    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("chargingCentral"));

    auto *header = new QWidget(central);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(20, 18, 20, 12);
    headerLayout->setSpacing(10);
    auto *logo = new QLabel(QStringLiteral("⚡"), header);
    logo->setObjectName(QStringLiteral("brandMark"));
    logo->setFixedSize(40, 40);
    logo->setAlignment(Qt::AlignCenter);
    auto *brand = new QLabel(QStringLiteral("NCS Charge"), header);
    brand->setObjectName(QStringLiteral("brandName"));
    auto *caption = new QLabel(QStringLiteral("SMART CHARGING PLATFORM"), header);
    caption->setObjectName(QStringLiteral("brandCaption"));
    auto *brandColumn = new QVBoxLayout;
    brandColumn->setSpacing(2);
    brandColumn->addWidget(brand);
    brandColumn->addWidget(caption);
    headerLayout->addWidget(logo);
    headerLayout->addLayout(brandColumn);
    headerLayout->addStretch();
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(header);
    rootLayout->addWidget(m_stack, 1);
    rootLayout->addLayout(navBar);
    setCentralWidget(central);
}

void MainWindow::onStationClicked(const core::StationListItem &item)
{
    if (!checkChargingEntry(this)) {
        return;
    }
    QDialog *dialog = buildStationDetailDialog(item);
    dialog->exec();
    dialog->deleteLater();
}

QDialog *MainWindow::buildStationDetailDialog(
    const core::StationListItem &item)
{
    auto *dialog = new QDialog(this);
    dialog->setWindowTitle(QStringLiteral("电站详情"));
    dialog->setModal(true);
    dialog->resize(420,760);

    auto *mainLayout = new QVBoxLayout(dialog);
    mainLayout->setContentsMargins(12, 16, 12, 16);
    mainLayout->setSpacing(10);

    // ==============================
    // 1. 顶部：电站基本信息
    // ==============================

    auto *nameLabel = new QLabel(item.name, dialog);
    nameLabel->setTextFormat(Qt::PlainText);
    nameLabel->setWordWrap(true);
    nameLabel->setStyleSheet(
        "color: #F2F5FA;"
        "font-size: 22px;"
        "font-weight: bold;"
    );

    mainLayout->addWidget(nameLabel);

    auto *formLayout = new QFormLayout;
    formLayout->setHorizontalSpacing(14);
    formLayout->setVerticalSpacing(10);

    auto *addressLabel = new QLabel(item.address, dialog);
    addressLabel->setTextFormat(Qt::PlainText);
    addressLabel->setWordWrap(true);

    auto *priceLabel = new QLabel(
        QStringLiteral("%1 元/度")
            .arg(item.price, 0, 'f', 2),
        dialog
    );

    auto *distanceLabel = new QLabel(
        QStringLiteral("%1 公里")
            .arg(item.distanceKm, 0, 'f', 1),
        dialog
    );

    formLayout->addRow(
        QStringLiteral("详细地址："), addressLabel);

    formLayout->addRow(
        QStringLiteral("充电单价："), priceLabel);

    formLayout->addRow(
        QStringLiteral("距离："), distanceLabel);

    mainLayout->addLayout(formLayout);

    auto *sectionLabel = new QLabel(
        QStringLiteral("本站电桩"), dialog);

    sectionLabel->setStyleSheet(
        "color: #BFDBFE;"
        "font-size: 16px;"
        "font-weight: bold;"
    );

    mainLayout->addWidget(sectionLabel);

    // ==============================
    // 2. 中部：本站电桩表格
    // ==============================

    auto *table = new QTableWidget(dialog);

    table->setColumnCount(6);
    table->setHorizontalHeaderLabels(
        QStringList{
            QStringLiteral("编号"),
            QStringLiteral("类型"),
            QStringLiteral("功率\n(kW)"),
            QStringLiteral("状态"),
            QStringLiteral("累计\n次数"),
            QStringLiteral("电桩ID")
        }
    );

    // ID保留在表格中，供后面选桩充电使用，
    // 当前界面不显示这一列。
    table->setColumnHidden(5, true);

    table->setEditTriggers(
        QAbstractItemView::NoEditTriggers);

    table->setSelectionBehavior(
        QAbstractItemView::SelectRows);

    table->setSelectionMode(
        QAbstractItemView::SingleSelection);

    table->setAlternatingRowColors(true);
    table->setShowGrid(false);

    table->verticalHeader()->hide();
    table->verticalHeader()->setDefaultSectionSize(44);

    table->horizontalHeader()->setMinimumSectionSize(45);
    table->horizontalHeader()->setSectionResizeMode(
        QHeaderView::Stretch);

    // 允许表格缩小，多余行通过上下滚动查看。
    table->setMinimumWidth(0);
    table->setMinimumHeight(0);
    table->setWordWrap(true);

    table->setStyleSheet(R"(
        QTableWidget {
            background: #111E33;
            alternate-background-color: #16263D;
            color: #F2F5FA;
            border: 1px solid #293C55;
            border-radius: 8px;
            font-size: 11px;
        }

        QTableWidget::item {
            padding: 2px;
        }

        QTableWidget::item:selected {
            background: #274363;
        }

        QHeaderView::section {
            background: #1B304C;
            color: #BFDBFE;
            border: none;
            padding: 6px 2px;
            font-size: 11px;
            font-weight: bold;
        }

        QTableCornerButton::section {
            background: #1B304C;
            border: none;
        }
    )");

    table->horizontalHeader()->setStyleSheet(R"(
        QHeaderView {
            background-color: #1B304C;
        }

        QHeaderView::section {
            background-color: #1B304C;
            color: #BFDBFE;
            border: 1px solid #293C55;
            padding: 6px 2px;
            font-size: 11px;
            font-weight: bold;
        }
    )");

    table->horizontalHeader()->setFixedHeight(42);

    mainLayout->addWidget(table, 1);

    // 查询结果或错误提示
    auto *messageLabel = new QLabel(dialog);
    messageLabel->setTextFormat(Qt::PlainText);
    messageLabel->setWordWrap(true);
    mainLayout->addWidget(messageLabel);

    // ==============================
    // 3. 查询并填充表格
    // ==============================

    // 保存当前点击的电站ID。
    const int stationId = item.id;

    auto reloadTable =
        [this, stationId, table, messageLabel]()
    {
        table->setRowCount(0);

        QVector<core::ChargerData> chargers;
        QString errorMessage;

        // 界面调用服务层，服务层再调用数据库查询。
        const bool success =
            m_service->getChargersByStationId(
                stationId,
                chargers,
                &errorMessage
            );

        if (!success) {
            messageLabel->setStyleSheet(
                "color: #F87171;"
            );

            messageLabel->setText(
                QStringLiteral("读取电桩失败：%1")
                    .arg(errorMessage)
            );
            return;
        }

        if (chargers.isEmpty()) {
            messageLabel->setStyleSheet(
                "color: #94A3B8;"
            );

            messageLabel->setText(
                QStringLiteral("该电站暂无电桩")
            );
            return;
        }

        table->setRowCount(
            static_cast<int>(chargers.size()));

        int freeCount = 0;

        for (int row = 0; row < chargers.size(); ++row) {
            const core::ChargerData &charger =
                chargers.at(row);

            // 类型：0=慢充，1=快充。
            QString typeText;

            if (charger.type == 0) {
                typeText = QStringLiteral("慢充");
            } else if (charger.type == 1) {
                typeText = QStringLiteral("快充");
            } else {
                typeText = QStringLiteral("未知");
            }

            // 状态文字与颜色。
            QString stateText;
            QColor stateColor;

            switch (charger.status) {
            case 0:
                stateText = QStringLiteral("空闲");
                stateColor = QColor("#4ADE80");
                ++freeCount;
                break;

            case 1:
                stateText = QStringLiteral("使用中");
                stateColor = QColor("#FB923C");
                break;

            case 2:
                stateText = QStringLiteral("故障");
                stateColor = QColor("#F87171");
                break;

            default:
                stateText = QStringLiteral("未知");
                stateColor = QColor("#94A3B8");
                break;
            }

            const QStringList values{
                charger.chargerNo,
                typeText,
                QString::number(charger.power, 'f', 1),
                stateText,
                QString::number(charger.totalCount),
                QString::number(charger.id)
            };

            for (int column = 0;
                 column < values.size();
                 ++column) {

                auto *cell = new QTableWidgetItem(
                    values.at(column));

                cell->setToolTip(values.at(column));

                table->setItem(row, column, cell);
            }

            // 第3列是状态列，列编号从0开始。
            table->item(row, 3)->setForeground(
                stateColor);
        }

        messageLabel->setStyleSheet(
            "color: #94A3B8;"
        );

        messageLabel->setText(
            QStringLiteral(
                "共 %1 根电桩，当前空闲 %2 根。")
                .arg(static_cast<int>(chargers.size()))
                .arg(freeCount)
        );
    };

    // ==============================
    // 4. 底部：刷新与关闭
    // ==============================

    auto *buttons = new QDialogButtonBox(dialog);

    // 一键导航按钮
    auto *navigationButton = buttons->addButton(
        QStringLiteral("一键导航"),
        QDialogButtonBox::ActionRole
    );

    navigationButton->setCursor(Qt::PointingHandCursor);

    connect(
        navigationButton,
        &QPushButton::clicked,
        dialog,
        [this, item, dialog, messageLabel]()
        {
            // 尚未完成定位时，先提示用户。
            if (m_service->lastRegionName().isEmpty()) {
                messageLabel->setStyleSheet(
                    "color: #F87171;"
                );

                messageLabel->setText(
                    QStringLiteral("请先返回首页完成定位。")
                );
                return;
            }

            // 起点使用首页最近一次定位的坐标。
            // 用通用名称，避免把输入地址定位点误写成区域中心。
            NavigationDialog navigationDialog(
                item,
                m_service->lastLatitude(),
                m_service->lastLongitude(),
                QStringLiteral("首页定位点"),
                dialog
            );

            navigationDialog.exec();
        }
    );

    auto *refreshButton = buttons->addButton(
        QStringLiteral("刷新电桩"),
        QDialogButtonBox::ActionRole
    );

    auto *closeButton = buttons->addButton(
        QStringLiteral("关闭"),
        QDialogButtonBox::RejectRole
    );

    refreshButton->setCursor(Qt::PointingHandCursor);
    closeButton->setCursor(Qt::PointingHandCursor);

    connect(
        refreshButton,
        &QPushButton::clicked,
        dialog,
        reloadTable
    );

    connect(
        buttons,
        &QDialogButtonBox::rejected,
        dialog,
        &QDialog::reject
    );

    // ==============================
    // 选桩充电按钮
    // ==============================

    auto *selectChargerButton = new QPushButton(
        QStringLiteral("预约所选电桩"),
        dialog
    );

    selectChargerButton->setCursor(Qt::PointingHandCursor);
    selectChargerButton->setMinimumHeight(44);

    selectChargerButton->setStyleSheet(R"(
        QPushButton {
            background-color: #2563EB;
            color: white;
            border: 1px solid #3B82F6;
            border-radius: 10px;
            font-size: 14px;
            font-weight: bold;
        }

        QPushButton:hover {
            background-color: #3B82F6;
        }

        QPushButton:pressed {
            background-color: #1D4ED8;
        }
    )");

    mainLayout->addWidget(selectChargerButton);

    connect(
        selectChargerButton,
        &QPushButton::clicked,
        dialog,
        [this, stationId, table, messageLabel, reloadTable, dialog]()
        {
            if (!checkChargingEntry(dialog)) {
                return;
            }
            // 1. 检查用户是否选中一行。
            const int row = table->currentRow();

            if (row < 0 || table->selectedItems().isEmpty()) {
                messageLabel->setStyleSheet(
                    "color: #FB923C;"
                );
                messageLabel->setText(
                    QStringLiteral("请先选择一根电桩。")
                );
                return;
            }

            // 2. 从隐藏的第5列读取电桩ID。
            // 注意：列编号从0开始，第5列是第六列。
            const QTableWidgetItem *idItem =
                table->item(row, 5);

            if (!idItem) {
                messageLabel->setStyleSheet(
                    "color: #F87171;"
                );
                messageLabel->setText(
                    QStringLiteral("无法读取电桩ID，请刷新后重试。")
                );
                return;
            }

            bool validId = false;
            const int chargerId =
                idItem->text().toInt(&validId);

            if (!validId || chargerId <= 0) {
                messageLabel->setStyleSheet(
                    "color: #F87171;"
                );
                messageLabel->setText(
                    QStringLiteral("电桩ID无效，请刷新后重试。")
                );
                return;
            }

            // 3. 重新查询数据库。
            // 不能只相信表格里的旧状态：
            // 管理端可能在打开详情后修改过这根桩。
            QVector<core::ChargerData> latestChargers;
            QString errorMessage;

            const bool success =
                m_service->getChargersByStationId(
                    stationId,
                    latestChargers,
                    &errorMessage
                );

            if (!success) {
                messageLabel->setStyleSheet(
                    "color: #F87171;"
                );
                messageLabel->setText(
                    QStringLiteral("检查电桩状态失败：%1")
                        .arg(errorMessage)
                );
                return;
            }

            // 4. 在本站最新电桩记录中寻找所选电桩。
            bool found = false;
            core::ChargerData selectedCharger;

            for (const core::ChargerData &charger
                 : latestChargers) {

                if (charger.id == chargerId) {
                    selectedCharger = charger;
                    found = true;
                    break;
                }
            }

            if (!found) {
                reloadTable();

                messageLabel->setStyleSheet(
                    "color: #F87171;"
                );
                messageLabel->setText(
                    QStringLiteral(
                        "该电桩已不存在或不属于本站，"
                        "请重新选择。")
                );
                return;
            }

            // 5. 只有数据库中仍为空闲的电桩可以继续。
            if (selectedCharger.status != 0) {
                reloadTable();

                QString reason;

                switch (selectedCharger.status) {
                case 1:
                    reason = QStringLiteral(
                        "该电桩正在使用中，请选择其他空闲电桩。");
                    break;

                case 2:
                    reason = QStringLiteral(
                        "该电桩处于故障状态，请选择其他空闲电桩。");
                    break;

                default:
                    reason = QStringLiteral(
                        "该电桩状态异常，暂时无法选择。");
                    break;
                }

                messageLabel->setStyleSheet(
                    "color: #FB923C;"
                );
                messageLabel->setText(reason);
                return;
            }

            // 6. 检查通过，请求确认并开始充电。
            messageLabel->clear();

            emit chargingRequested(
                stationId,
                selectedCharger.id
            );

            // 信号处理结束、订单窗口关闭后，刷新详情表格。
            reloadTable();
        }
    );

    mainLayout->addWidget(buttons);

    // 第一次打开详情弹窗时立即加载。
    reloadTable();

    return dialog;
}

void MainWindow::setupStyle()
{
    // 与登录页、个人主页统一：深蓝背景、蓝色强调、圆角卡片。
    setStyleSheet(QStringLiteral(R"(
        QWidget {
            font-family: "Noto Sans CJK SC", "Microsoft YaHei", "PingFang SC", sans-serif;
            font-size: 13px; color: #F2F5FA;
        }
        QMainWindow, QDialog, QWidget#chargingCentral, QStackedWidget {
            background: #0B1220;
        }
        QLabel { background: transparent; }
        QLabel#brandMark {
            background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #3B82F6,stop:1 #35D7FF);
            color: white; border-radius: 20px; font-size: 22px; font-weight: bold;
        }
        QLabel#brandName { font-size: 18px; font-weight: bold; }
        QLabel#brandCaption { color: #8A93A6; font-size: 9px; }
        QLabel#pageTitle { font-size: 25px; font-weight: bold; }
        QLabel#pageSubtitle { color: #8FA3BC; font-size: 12px; }
        QLabel#sectionTitle { font-size: 15px; font-weight: bold; }
        QFrame#locationPanel {
            background: #111E33; border: 1px solid #22344F; border-radius: 16px;
        }
        QComboBox, QLineEdit {
            background: #0F1A30; color: #F2F5FA;
            border: 1px solid #2A3C57; border-radius: 10px;
            padding: 9px 10px; min-height: 20px;
            selection-background-color: #3B82F6; selection-color: white;
        }
        QComboBox:focus, QLineEdit:focus { border-color: #60A5FA; }
        QComboBox::drop-down { border: none; width: 26px; }
        QComboBox QAbstractItemView {
            background: #14233B; color: #F2F5FA; border: 1px solid #2A3C57;
            selection-background-color: #2556A0; selection-color: white;
            outline: none;
        }
        QPushButton {
            background: #14243C; color: #BFDBFE; border: 1px solid #2A4364;
            border-radius: 10px; padding: 8px 12px; min-height: 20px;
        }
        QPushButton:hover { background: #1C3354; border-color: #60A5FA; }
        QPushButton:pressed { background: #24456E; }
        QPushButton:focus { border-color: #93C5FD; }
        QPushButton#locateBtn, QPushButton#navBtn:checked {
            background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #2563EB,stop:1 #3B82F6);
            color: white; border: 1px solid #3B82F6; font-weight: bold;
        }
        QPushButton#locateBtn:hover, QPushButton#navBtn:checked:hover { background: #397EF1; }
        QPushButton#locateBtn:pressed { background: #1D4ED8; }
        QPushButton:disabled { background: #1C2C45; color: #788CA6; border-color: #263853; }
        QPushButton#navBtn { padding: 12px 0; font-size: 14px; border-radius: 12px; }
        QLabel#locationLabel { color: #99ACC3; font-size: 11px; }
        QLabel#toastLabel {
            background: #102D43; color: #7DD3FC;
            border: 1px solid #205471; border-radius: 10px; padding: 8px;
        }
        QLabel#toastLabel[error="true"] {
            background: #36202C; color: #FDA4AF; border-color: #653348;
        }
        QFrame#stationCard {
            background: #111E33; border: 1px solid #22344F; border-radius: 16px;
        }
        QFrame#stationCard:hover { background: #15253E; border-color: #3B82F6; }
        QLabel#stationName { font-size: 15px; font-weight: bold; color: #F2F5FA; }
        QLabel#stationAddress { color: #93A5BD; font-size: 11px; }
        QLabel#priceLabel { color: #60A5FA; font-weight: bold; font-size: 16px; }
        QLabel#freeLabel {
            background: #10372F; color: #6EE7B7; border-radius: 7px;
            padding: 4px 7px; font-size: 11px; font-weight: bold;
        }
        QLabel#freeLabelFull {
            background: #3B2431; color: #FDA4AF; border-radius: 7px;
            padding: 4px 7px; font-size: 11px;
        }
        QLabel#distLabel { color: #7DD3FC; font-size: 12px; font-weight: bold; }
        QLabel#emptyHint { color: #99ACC3; font-size: 14px; }
        QLabel#detailNote { color: #99ACC3; font-size: 11px; }
        QScrollArea#cardScrollArea { background: transparent; border: none; }
        QWidget#cardsContainer { background: #0B1220; }
        QScrollBar:vertical { background: #0B1220; width: 5px; margin: 0; }
        QScrollBar::handle:vertical { background: #314869; min-height: 30px; border-radius: 2px; }
        QScrollBar::handle:vertical:hover { background: #60A5FA; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
    )"));
}


void MainWindow::setCurrentUser(const UserInfo &user)
{
    m_currentUser = user;

    setWindowTitle(
        QStringLiteral("NCS 充电 - %1").arg(user.nickname)
    );

    m_stack->setCurrentIndex(0);
    m_navStationBtn->setChecked(true);
}

bool MainWindow::checkChargingEntry(QWidget *messageParent)
{
    UserService userService;
    int orderId = -1;
    QString errorMessage;

    const bool success = userService.findUnfinishedOrder(
        m_currentUser.id,
        orderId,
        errorMessage
    );

    // 查询失败也不能放行。
    if (!success) {
        QMessageBox::warning(
            messageParent,
            QStringLiteral("无法进入充电"),
            errorMessage
        );
        return false;
    }

    // 没有未完成订单，允许继续。
    if (orderId == -1) {
        return true;
    }

    QMessageBox messageBox(messageParent);
    messageBox.setWindowTitle(QStringLiteral("未完成订单"));
    messageBox.setIcon(QMessageBox::Warning);
    messageBox.setText(
        QStringLiteral("您有未完成的充电订单，请先结算")
    );

    messageBox.setStandardButtons(QMessageBox::NoButton);

    auto *settleButton = messageBox.addButton(
        QStringLiteral("去结算"),
        QMessageBox::AcceptRole
    );

    messageBox.setDefaultButton(settleButton);
    messageBox.setWindowFlag(
        Qt::WindowCloseButtonHint,
        false
    );

    messageBox.exec();

    // 即使通过系统方式关闭提示，也不能继续选桩。
    emit settlementRequested(orderId);
    return false;
}
