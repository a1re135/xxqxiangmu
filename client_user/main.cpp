#include "mainwindow.h"
#include "loginwindow.h"
#include "database/databasemanager.h"

#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    DatabaseManager databaseManager;

    if (!databaseManager.initialize()) {

        QMessageBox::critical(
            nullptr,
            "数据库错误",
            "数据库初始化失败，请检查数据库文件和权限。"
        );

        return -1;
    }

    LoginWindow loginWindow;
    MainWindow mainWindow;

    QObject::connect(
        &loginWindow,
        &LoginWindow::loginSucceeded,
        &mainWindow,
        [&mainWindow](const UserInfo &user) {

            mainWindow.setCurrentUser(user);
            mainWindow.show();
        }
    );

    loginWindow.show();

    return a.exec();
}
