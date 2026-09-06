#include "adminloginwindow.h"
#include "mainwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include "database/databasemanager.h"
#include "logging/ncslogger.h"
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

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("NCS"));
    QCoreApplication::setApplicationName(QStringLiteral("NCS_Charging_Platform"));
    NcsLogger::install();

    DatabaseManager databaseManager;
    if (!databaseManager.initialize()) {
        QMessageBox::critical(
            nullptr,
            QStringLiteral("数据库错误"),
            QStringLiteral(
                "数据库初始化失败。\n"
                "请检查数据库文件、目录权限或数据库配置。"));
        return -1;
    }

    AdminLoginWindow loginWindow;
    MainWindow mainWindow;

    QObject::connect(
        &loginWindow,
        &AdminLoginWindow::loginSucceeded,
        &mainWindow,
        [&mainWindow](const AdminAuthService::AdminInfo &admin) {
            mainWindow.setCurrentAdmin(admin);
            mainWindow.show();
            mainWindow.raise();
            mainWindow.activateWindow();
        });

    QObject::connect(
        &mainWindow,
        &MainWindow::logoutRequested,
        &loginWindow,
        [&loginWindow]() {
            loginWindow.resetForLogout();
            loginWindow.show();
            loginWindow.raise();
            loginWindow.activateWindow();
        });

    loginWindow.show();
    return a.exec();
}
