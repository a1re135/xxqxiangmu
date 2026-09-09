#include "mainwindow.h"
#include "loginwindow.h"
#include "personalhomepage.h"

#include "ui/station_list_page.h"

#include "database/databasemanager.h"
#include "service/stationservice.h"

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

    MainWindow mainWindow(&stationService);

    PersonalHomePage personalPage(
        loginWindow.userService()
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

            mainWindow.hide();
            personalPage.hide();

            // IMPORTANT:
            // After login we now go to MainWindow,
            // not directly to PersonalHomePage.
            mainWindow.show();
            mainWindow.raise();
            mainWindow.activateWindow();
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

    // =========================================================
    // Start application
    // =========================================================
    loginWindow.show();

    return a.exec();
}
