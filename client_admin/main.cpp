#include "adminloginwindow.h"
#include "mainwindow.h"

#include <QApplication>
#include <QMessageBox>

#include "database/databasemanager.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    DatabaseManager databaseManager;

    if (!databaseManager.initialize()) {

        QMessageBox::critical(
            nullptr,
            QStringLiteral("数据库错误"),
            QStringLiteral(
                "数据库初始化失败。\n"
                "请检查数据库文件、目录权限或数据库配置。"
            )
        );

        return -1;
    }

    AdminLoginWindow loginWindow;
    MainWindow mainWindow;

    QObject::connect(
        &loginWindow,
        &AdminLoginWindow::loginSucceeded,
        &mainWindow,
        [&mainWindow](
            const AdminAuthService::AdminInfo &admin)
        {
            mainWindow.setCurrentAdmin(admin);
            mainWindow.show();
        }
    );

    loginWindow.show();

    return a.exec();
}
