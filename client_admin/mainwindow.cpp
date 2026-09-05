#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle(
        QStringLiteral("东软电动汽车充电桩应用管理平台")
    );
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setCurrentAdmin(
    const AdminAuthService::AdminInfo &admin)
{
    m_currentAdmin = admin;

    setWindowTitle(
        QStringLiteral(
            "东软电动汽车充电桩应用管理平台 - 管理员：%1"
        ).arg(m_currentAdmin.username)
    );
}
