#include "mainwindow.h"
#include "loginwindow.h"
#include "personalhomepage.h"

#include "ui/station_list_page.h"

#include "database/databasemanager.h"
#include "service/station_service.h"

#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // =========================================================
    // Database
    // =========================================================
    DatabaseManager databaseManager;

    if (!databaseManager.initialize()) {
        QMessageBox::critical(
            nullptr,
            QStringLiteral("数据库错误"),
            QStringLiteral("数据库初始化失败，请检查数据库文件和权限。")
        );

        return -1;
    }

    // =========================================================
    // Services
    // =========================================================
    core::StationService stationService(&databaseManager);

    // =========================================================
    // Windows / pages
    // =========================================================
    LoginWindow loginWindow;

    MainWindow mainWindow;

    PersonalHomePage personalPage(
        loginWindow.userService()
    );

    client_user::StationListPage stationPage(
        &stationService
    );

    stationPage.setFixedSize(420, 760);
    stationPage.setWindowTitle(
        QStringLiteral("附近充电站")
    );

    UserInfo currentUser;

    // =========================================================
    // Helper: return to home page
    // =========================================================
    auto showMainWindow = [&]() {

        // Reload latest user information.
        UserInfo latest;
        QString error;

        if (currentUser.id > 0 &&
            loginWindow.userService().getUserById(
                currentUser.id,
                latest,
                error
            )) {

            currentUser = latest;
        }

        mainWindow.setCurrentUser(currentUser);

        loginWindow.hide();
        personalPage.hide();
        stationPage.hide();

        mainWindow.show();
        mainWindow.raise();
        mainWindow.activateWindow();
    };

    // =========================================================
    // LOGIN SUCCESS
    // =========================================================
    QObject::connect(
        &loginWindow,
        &LoginWindow::loginSucceeded,

        [&](const UserInfo &user) {

            currentUser = user;

            mainWindow.setCurrentUser(currentUser);
            personalPage.setUser(currentUser);

            loginWindow.hide();
            personalPage.hide();
            stationPage.hide();

            // IMPORTANT:
            // After login we now go to MainWindow,
            // not directly to PersonalHomePage.
            mainWindow.show();
            mainWindow.raise();
            mainWindow.activateWindow();
        }
    );

    // =========================================================
    // HOME → JIAQI'S STATION PAGE
    // =========================================================
    QObject::connect(
        &mainWindow,
        &MainWindow::stationListRequested,

        [&]() {

            mainWindow.hide();

            stationPage.show();
            stationPage.raise();
            stationPage.activateWindow();
        }
    );

    // =========================================================
    // STATION PAGE → HOME
    // =========================================================
    QObject::connect(
        &stationPage,
        &client_user::StationListPage::backToHomeRequested,

        [&]() {
            showMainWindow();
        }
    );

    // =========================================================
    // HOME → HUIYEN'S PERSONAL CENTER
    // =========================================================
    QObject::connect(
        &mainWindow,
        &MainWindow::personalCenterRequested,

        [&]() {

            // Always refresh user information before opening.
            UserInfo latest;
            QString error;

            if (loginWindow.userService().getUserById(
                    currentUser.id,
                    latest,
                    error)) {

                currentUser = latest;
            }

            personalPage.setUser(currentUser);

            mainWindow.hide();

            personalPage.show();
            personalPage.raise();
            personalPage.activateWindow();
        }
    );

    // =========================================================
    // PERSONAL CENTER → HOME
    // =========================================================
    QObject::connect(
        &personalPage,
        &PersonalHomePage::backToHomeRequested,

        [&]() {
            showMainWindow();
        }
    );

    // =========================================================
    // LOGOUT
    // =========================================================
    auto returnToLogin = [&]() {

        mainWindow.hide();
        personalPage.hide();
        stationPage.hide();

        currentUser = UserInfo{};

        loginWindow.show();
        loginWindow.raise();
        loginWindow.activateWindow();
    };

    QObject::connect(
        &personalPage,
        &PersonalHomePage::logoutRequested,
        returnToLogin
    );

    QObject::connect(
        &mainWindow,
        &MainWindow::logoutRequested,
        returnToLogin
    );

    // =========================================================
    // Start application
    // =========================================================
    loginWindow.show();

    return a.exec();
}
