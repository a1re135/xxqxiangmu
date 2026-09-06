#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_welcomeLabel(nullptr)
    , m_balanceLabel(nullptr)
{
    ui->setupUi(this);
    setFixedSize(420, 760);
    setWindowTitle(QStringLiteral("充电平台首页"));

    // FIX: 原 MainWindow 是空白页，这里先提供一个最小可用首页。
    auto *root = new QVBoxLayout(ui->centralwidget);
    root->setContentsMargins(28, 32, 28, 32);
    root->setSpacing(18);

    auto *title = new QLabel(QStringLiteral("⚡ NCS 智慧充电"), ui->centralwidget);
    title->setStyleSheet("font-size:26px;font-weight:700;");
    root->addWidget(title);

    m_welcomeLabel = new QLabel(QStringLiteral("欢迎回来"), ui->centralwidget);
    m_welcomeLabel->setStyleSheet("font-size:20px;font-weight:600;");
    root->addWidget(m_welcomeLabel);

    m_balanceLabel = new QLabel(QStringLiteral("当前余额：¥ 0.00"), ui->centralwidget);
    m_balanceLabel->setStyleSheet("font-size:16px;padding:12px;background:#F4F7FB;border-radius:10px;");
    root->addWidget(m_balanceLabel);

    auto *stationBtn = new QPushButton(QStringLiteral("查看附近充电站"), ui->centralwidget);
    auto *profileBtn = new QPushButton(QStringLiteral("个人中心"), ui->centralwidget);
    auto *logoutBtn = new QPushButton(QStringLiteral("退出登录"), ui->centralwidget);

    for (QPushButton *button : {stationBtn, profileBtn, logoutBtn}) {
        button->setMinimumHeight(48);
        button->setCursor(Qt::PointingHandCursor);
        root->addWidget(button);
    }

    connect(
        stationBtn,
        &QPushButton::clicked,
        this,
        &MainWindow::stationListRequested
    );
    connect(profileBtn, &QPushButton::clicked, this, &MainWindow::personalCenterRequested);
    connect(logoutBtn, &QPushButton::clicked, this, &MainWindow::logoutRequested);

    root->addStretch();

    auto *hint = new QLabel(QStringLiteral("当前为可运行的临时首页，后续可继续接入充电站列表/地图。"), ui->centralwidget);
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#6B7280;font-size:12px;");
    root->addWidget(hint);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setCurrentUser(const UserInfo &user)
{
    m_currentUser = user;
    setWindowTitle(QStringLiteral("充电用户端 - %1").arg(m_currentUser.nickname));

    if (m_welcomeLabel) {
        m_welcomeLabel->setText(QStringLiteral("欢迎回来，%1").arg(m_currentUser.nickname));
    }
    if (m_balanceLabel) {
        m_balanceLabel->setText(QStringLiteral("当前余额：¥ %1").arg(m_currentUser.balance, 0, 'f', 2));
    }
}
