#ifndef ADMINLOGINWINDOW_H
#define ADMINLOGINWINDOW_H

#include <QMainWindow>
#include <QTimer>

#include "service/adminauthservice.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class AdminLoginWindow;
}
QT_END_NAMESPACE

class AdminLoginWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit AdminLoginWindow(QWidget *parent = nullptr);
    ~AdminLoginWindow();

signals:
    void loginSucceeded(const AdminAuthService::AdminInfo &admin);

private slots:
    void onLoginClicked();
    void onTogglePasswordClicked();
    void updateLockoutCountdown();

private:
    enum class LoginState
    {
        Ready,
        Authenticating,
        Locked
    };

    void setStatusMessage(const QString &message, bool error = true);
    void setLoginEnabled(bool enabled);
    void enterLockout();
    void leaveLockout();
    void clearPasswordFeedback();
    bool validateInput(QString &username, QString &password);

    Ui::AdminLoginWindow *ui;

    AdminAuthService m_authService;
    QTimer *m_lockoutTimer;

    int m_failedAttempts;
    int m_lockoutRemainingSeconds;
    LoginState m_state;
};

#endif // ADMINLOGINWINDOW_H
