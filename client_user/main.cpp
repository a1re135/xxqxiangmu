#include "mainwindow.h"
#include "loginwindow.h"
#include "personalhomepage.h"
#include "database/databasemanager.h"

#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    DatabaseManager databaseManager;
    if (!databaseManager.initialize()) {
        QMessageBox::critical(nullptr, QStringLiteral("数据库错误"),
                              QStringLiteral("数据库初始化失败，请检查数据库文件和权限。"));
        return -1;
    }

    LoginWindow loginWindow;
    MainWindow mainWindow;
    PersonalHomePage personalPage(loginWindow.userService());

    UserInfo currentUser;

    // FIX: 窗口切换全部集中在 main.cpp，保证同一时刻只展示正确页面。
    QObject::connect(&loginWindow, &LoginWindow::loginSucceeded,
                     [&](const UserInfo &user) {
        currentUser = user;
        personalPage.setUser(currentUser);
        loginWindow.hide();
        mainWindow.hide();
        personalPage.show();
        personalPage.raise();
        personalPage.activateWindow();
    });

    QObject::connect(&personalPage, &PersonalHomePage::backToHomeRequested,
                     [&]() {
        UserInfo latest;
        QString error;
        if (loginWindow.userService().getUserById(currentUser.id, latest, error)) {
            currentUser = latest;
        }
        mainWindow.setCurrentUser(currentUser);
        personalPage.hide();
        mainWindow.show();
        mainWindow.raise();
        mainWindow.activateWindow();
    });

    QObject::connect(&mainWindow, &MainWindow::personalCenterRequested,
                     [&]() {
        UserInfo latest;
        QString error;
        if (loginWindow.userService().getUserById(currentUser.id, latest, error)) {
            currentUser = latest;
            personalPage.setUser(currentUser);
        }
        mainWindow.hide();
        personalPage.show();
        personalPage.raise();
        personalPage.activateWindow();
    });

    auto returnToLogin = [&]() {
        personalPage.hide();
        mainWindow.hide();
        currentUser = UserInfo{};
        loginWindow.show();
        loginWindow.raise();
        loginWindow.activateWindow();
    };

    QObject::connect(&personalPage, &PersonalHomePage::logoutRequested, returnToLogin);
    QObject::connect(&mainWindow, &MainWindow::logoutRequested, returnToLogin);

    loginWindow.show();
    return a.exec();
}
