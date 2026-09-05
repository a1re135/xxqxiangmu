#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QMainWindow>
#include <QTimer>

#include "service/userservice.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class LoginWindow;
}
QT_END_NAMESPACE

class LoginWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit LoginWindow(QWidget *parent = nullptr);
    ~LoginWindow();

signals:
    void loginSucceeded(const UserInfo &user);

private slots:
    void onGetCodeClicked();
    void onLoginClicked();
    void updateCountdown();

private:
    bool validatePhone(QString &phone);
    void setStatusMessage(const QString &message, bool error = true);
    void resetVerificationState();

    Ui::LoginWindow *ui;
    UserService m_userService;
    QTimer *m_countdownTimer;

    QString m_currentVerificationCode;
    int m_remainingSeconds;
};

#endif // LOGINWINDOW_H
