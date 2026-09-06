#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "ui/station_list_page.h"

MainWindow::MainWindow(core::StationService *stationService,
                       QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_stationService(stationService)
{
    ui->setupUi(this);

    setFixedSize(420, 760);
    setWindowTitle("NCS 充电用户端");

    auto *stationPage =
        new client_user::StationListPage(m_stationService, this);

    setCentralWidget(stationPage);
}

MainWindow::~MainWindow()
{
    delete ui;
}
