#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setCurrentUser(const UserInfo &user)
{
    m_currentUser = user;

    setWindowTitle(
        "充电用户端 - " +
        m_currentUser.nickname
    );
}
