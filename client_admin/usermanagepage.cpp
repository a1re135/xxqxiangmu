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

void MainWindow::setupUserManagement()
{
    auto *page = uiObject<QWidget>(this, QStringLiteral("userManagePage"));
    auto *rootLayout = uiObject<QVBoxLayout>(this, QStringLiteral("userManagePageLayout"));
    if (!page || !rootLayout) {
        return;
    }

    while (rootLayout->count() > 0) {
        QLayoutItem *item = rootLayout->takeAt(0);
        if (QWidget *widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }

    rootLayout->setContentsMargins(28, 24, 28, 24);
    rootLayout->setSpacing(16);

    auto *header = new QHBoxLayout();
    header->setSpacing(14);

    auto *icon = new QLabel(QStringLiteral("♙"), page);
    icon->setFixedSize(48, 48);
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet(QStringLiteral(
        "QLabel { color:#10B981; background:#102D2B; border:1px solid #1B5148; "
        "border-radius:14px; font-size:24px; font-weight:900; }"));

    auto *titleBox = new QVBoxLayout();
    titleBox->setSpacing(2);

    auto *title = new QLabel(QStringLiteral("用户管理"), page);
    title->setStyleSheet(QStringLiteral(
        "QLabel { color:#F5F7FA; font-size:24px; font-weight:900; }"));

    auto *subtitle = new QLabel(
        QStringLiteral("查询用户账户、余额与状态，支持冻结、解冻及订单历史查看"),
        page);
    subtitle->setStyleSheet(QStringLiteral(
        "QLabel { color:#8FA1AC; font-size:14px; font-weight:600; }"));

    titleBox->addWidget(title);
    titleBox->addWidget(subtitle);

    header->addWidget(icon);
    header->addLayout(titleBox);
    header->addStretch();

    m_userSummaryLabel = new QLabel(QStringLiteral("共 0 位用户"), page);
    m_userSummaryLabel->setStyleSheet(QStringLiteral(
        "QLabel { color:#718394; background:#162336; border:1px solid #2B3A4D; "
        "border-radius:10px; padding:9px 12px; font-size:13px; font-weight:700; }"));
    header->addWidget(m_userSummaryLabel);

    rootLayout->addLayout(header);

    auto *filterCard = new QFrame(page);
    filterCard->setStyleSheet(QStringLiteral(
        "QFrame { background:#121D30; border:1px solid #25364A; border-radius:14px; }"));
    auto *filterLayout = new QHBoxLayout(filterCard);
    filterLayout->setContentsMargins(16, 12, 16, 12);
    filterLayout->setSpacing(10);

    auto *searchLabel = new QLabel(QStringLiteral("手机号"), filterCard);
    searchLabel->setStyleSheet(QStringLiteral(
        "QLabel { color:#93A4AE; font-size:14px; font-weight:800; }"));

    m_userSearch = new QLineEdit(filterCard);
    m_userSearch->setObjectName(QStringLiteral("userSearch"));
    m_userSearch->setPlaceholderText(QStringLiteral("输入手机号关键字，支持模糊搜索"));
    m_userSearch->setClearButtonEnabled(true);
    m_userSearch->setMinimumHeight(42);
    m_userSearch->setMinimumWidth(360);
    m_userSearch->setStyleSheet(QStringLiteral(
        "QLineEdit { background:#172438; color:#EEF4F6; border:1px solid #34495A; "
        "border-radius:9px; padding:0 14px; font-size:14px; } "
        "QLineEdit:focus { border:1px solid #10B981; }"));

    auto *searchButton = new QPushButton(QStringLiteral("搜索"), filterCard);
    searchButton->setMinimumSize(100, 42);
    searchButton->setStyleSheet(QStringLiteral(
        "QPushButton { background:#17372F; color:#54D9A7; border:1px solid #2B6C57; "
        "border-radius:9px; font-size:14px; font-weight:850; padding:0 18px; } "
        "QPushButton:hover { background:#1C453B; }"));

    m_userStatusButton = new QPushButton(QStringLiteral("冻结用户"), filterCard);
    m_userStatusButton->setMinimumSize(130, 42);
    m_userStatusButton->setEnabled(false);

    auto *orderButton = new QPushButton(QStringLiteral("订单历史"), filterCard);
    orderButton->setMinimumSize(130, 42);
    orderButton->setEnabled(false);

    const QString actionStyle = QStringLiteral(
        "QPushButton { background:#253244; color:#E6EDF0; border:1px solid #36475A; "
        "border-radius:9px; font-size:14px; font-weight:850; padding:0 18px; } "
        "QPushButton:hover { background:#2D3C4F; } "
        "QPushButton:disabled { color:#566772; background:#192535; border-color:#283746; }");
    m_userStatusButton->setStyleSheet(actionStyle);
    orderButton->setStyleSheet(actionStyle);

    filterLayout->addWidget(searchLabel);
    filterLayout->addWidget(m_userSearch);
    filterLayout->addWidget(searchButton);
    filterLayout->addSpacing(14);
    filterLayout->addWidget(m_userStatusButton);
    filterLayout->addWidget(orderButton);
    filterLayout->addStretch();

    rootLayout->addWidget(filterCard);

    auto *tableCard = new QFrame(page);
    tableCard->setStyleSheet(QStringLiteral(
        "QFrame { background:#121D30; border:1px solid #25364A; border-radius:14px; }"));
    auto *tableLayout = new QVBoxLayout(tableCard);
    tableLayout->setContentsMargins(14, 14, 14, 14);

    m_userTable = new QTableView(tableCard);
    m_userTable->setObjectName(QStringLiteral("userManagementTable"));
    m_userTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_userTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_userTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_userTable->setShowGrid(false);
    m_userTable->verticalHeader()->setVisible(false);
    m_userTable->horizontalHeader()->setStretchLastSection(true);
    m_userTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_userTable->setMinimumHeight(360);
    m_userTable->setStyleSheet(QStringLiteral(
        "QTableView { background:#101A2B; color:#EAF0F3; border:1px solid #27394D; "
        "border-radius:10px; font-size:14px; font-weight:600; selection-background-color:#153B35; "
        "selection-color:#F4F7F8; } "
        "QHeaderView::section { background:#1A2739; color:#8FA1AC; border:none; "
        "padding:13px 8px; font-size:13px; font-weight:800; } "
        "QTableView::item { padding:11px 8px; border-bottom:1px solid #1D2B3C; }"));

    m_userModel = new QStandardItemModel(0, 6, m_userTable);
    m_userModel->setHorizontalHeaderLabels({
        QStringLiteral("用户ID"),
        QStringLiteral("手机号"),
        QStringLiteral("昵称"),
        QStringLiteral("钱包余额(元)"),
        QStringLiteral("注册时间"),
        QStringLiteral("状态")
    });
    m_userTable->setModel(m_userModel);

    const int widths[] = {90, 170, 150, 150, 190, 110};
    for (int i = 0; i < 6; ++i) {
        m_userTable->setColumnWidth(i, widths[i]);
    }

    tableLayout->addWidget(m_userTable);
    rootLayout->addWidget(tableCard, 1);

    connect(searchButton, &QPushButton::clicked, this, &MainWindow::refreshUserManagement);
    connect(m_userSearch, &QLineEdit::returnPressed, this, &MainWindow::refreshUserManagement);
    connect(m_userSearch, &QLineEdit::textChanged, this, [this] {
        refreshUserManagement();
    });
    connect(m_userStatusButton, &QPushButton::clicked, this, &MainWindow::toggleSelectedUserStatus);
    connect(orderButton, &QPushButton::clicked, this, &MainWindow::showSelectedUserOrders);
    connect(m_userTable->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &MainWindow::userSelectionChanged);
    connect(m_userTable, &QTableView::doubleClicked, this,
            [this](const QModelIndex &) { showSelectedUserOrders(); });
}


void MainWindow::refreshUserManagement()
{
    if (!m_userTable || !m_userModel) {
        return;
    }

    QList<AdminUserService::UserRecord> users;
    QString errorMessage;
    if (!m_adminUserService.loadUsers(
            m_userSearch ? m_userSearch->text() : QString(),
            users,
            errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("用户管理"), errorMessage);
        return;
    }

    m_userRecords = users;
    m_userModel->removeRows(0, m_userModel->rowCount());

    for (const auto &u : users) {
        QList<QStandardItem *> row;
        auto *idItem = new QStandardItem(QString::number(u.id));
        auto *phoneItem = new QStandardItem(maskPhone(u.phone));
        auto *nicknameItem = new QStandardItem(u.nickname);
        auto *balanceItem = new QStandardItem(QString::number(u.balance, 'f', 2));
        auto *timeItem = new QStandardItem(u.registerTime);
        auto *statusItem = new QStandardItem(
            u.status == 0 ? QStringLiteral("冻结") : QStringLiteral("正常"));

        for (QStandardItem *item : row) {
            item->setTextAlignment(Qt::AlignCenter);
        }

        const QList<QStandardItem *> items = {
            idItem, phoneItem, nicknameItem, balanceItem, timeItem, statusItem
        };
        for (auto *item : items) {
            item->setTextAlignment(Qt::AlignCenter);
        }

        statusItem->setForeground(QBrush(QColor(
            u.status == 0 ? QStringLiteral("#F05252") : QStringLiteral("#10B981"))));

        m_userModel->appendRow(items);
    }

    m_userSummaryLabel->setText(
        QStringLiteral("当前显示 %1 位用户").arg(users.size()));

    if (users.isEmpty()) {
        m_selectedUserId = 0;
        m_userStatusButton->setEnabled(false);
        m_userStatusButton->setText(QStringLiteral("冻结用户"));
        for (auto *w : {static_cast<QWidget *>(m_userStatusButton)}) {
            Q_UNUSED(w)
        }
        return;
    }

    m_userTable->selectRow(0);
    m_selectedUserId = users.first().id;
    m_userStatusButton->setEnabled(true);
    m_userStatusButton->setText(
        users.first().status == 0 ? QStringLiteral("解冻用户") : QStringLiteral("冻结用户"));
}


void MainWindow::userSelectionChanged(const QModelIndex &current,
                                      const QModelIndex &previous)
{
    Q_UNUSED(previous);

    if (!current.isValid() || current.row() < 0 ||
        current.row() >= m_userRecords.size()) {
        m_selectedUserId = 0;
        if (m_userStatusButton) {
            m_userStatusButton->setEnabled(false);
            m_userStatusButton->setText(QStringLiteral("冻结用户"));
        }
        return;
    }

    const auto &user = m_userRecords.at(current.row());
    m_selectedUserId = user.id;
    m_userStatusButton->setEnabled(true);
    m_userStatusButton->setText(
        user.status == 0 ? QStringLiteral("解冻用户") : QStringLiteral("冻结用户"));
}


void MainWindow::toggleSelectedUserStatus()
{
    if (m_selectedUserId <= 0) {
        return;
    }

    auto it = std::find_if(
        m_userRecords.cbegin(), m_userRecords.cend(),
        [this](const AdminUserService::UserRecord &u) {
            return u.id == m_selectedUserId;
        });
    if (it == m_userRecords.cend()) {
        return;
    }

    const auto user = *it;
    const bool freeze = user.status != 0;

    if (freeze) {
        bool hasActive = false;
        QString errorMessage;
        if (!m_adminUserService.hasActiveChargingOrder(
                user.id, hasActive, errorMessage)) {
            QMessageBox::warning(this, QStringLiteral("用户状态"), errorMessage);
            return;
        }

        if (hasActive) {
            const auto result = QMessageBox::warning(
                this,
                QStringLiteral("冻结用户"),
                QStringLiteral("该用户有正在进行的充电，冻结后仍会保留订单"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);

            if (result != QMessageBox::Yes) {
                return;
            }
        } else {
            const auto result = QMessageBox::question(
                this,
                QStringLiteral("确认冻结"),
                QStringLiteral("确定冻结用户“%1”（%2）吗？")
                    .arg(user.nickname, maskPhone(user.phone)),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);

            if (result != QMessageBox::Yes) {
                return;
            }
        }
    } else {
        const auto result = QMessageBox::question(
            this,
            QStringLiteral("确认解冻"),
            QStringLiteral("确定解冻用户“%1”（%2）吗？")
                .arg(user.nickname, maskPhone(user.phone)),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (result != QMessageBox::Yes) {
            return;
        }
    }

    QString errorMessage;
    if (!m_adminUserService.setUserStatus(user.id, freeze ? 0 : 1, errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("用户状态"), errorMessage);
        return;
    }

    refreshUserManagement();
}

