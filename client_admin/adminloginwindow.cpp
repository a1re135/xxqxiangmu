#include "adminloginwindow.h"
#include "./ui_adminloginwindow.h"

#include <QLineEdit>
#include <QRegularExpression>
#include <QStyle>

AdminLoginWindow::AdminLoginWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::AdminLoginWindow)
    , m_lockoutTimer(new QTimer(this))
    , m_failedAttempts(0)
    , m_lockoutRemainingSeconds(0)
    , m_state(LoginState::Ready)
{
    ui->setupUi(this);

    setMinimumSize(1280, 800);
    resize(1280, 800);

    m_lockoutTimer->setInterval(1000);

    ui->statusLabel->clear();
    ui->passwordHintLabel->clear();

    connect(
        ui->loginButton,
        &QPushButton::clicked,
        this,
        &AdminLoginWindow::onLoginClicked
    );

    connect(
        ui->togglePasswordButton,
        &QPushButton::clicked,
        this,
        &AdminLoginWindow::onTogglePasswordClicked
    );

    connect(
        m_lockoutTimer,
        &QTimer::timeout,
        this,
        &AdminLoginWindow::updateLockoutCountdown
    );

    connect(
        ui->usernameLineEdit,
        &QLineEdit::returnPressed,
        this,
        [this] {
            ui->passwordLineEdit->setFocus();
        }
    );

    connect(
        ui->passwordLineEdit,
        &QLineEdit::returnPressed,
        this,
        &AdminLoginWindow::onLoginClicked
    );
}

AdminLoginWindow::~AdminLoginWindow()
{
    delete ui;
}

void AdminLoginWindow::resetForLogout()
{
    // A logout returns the login window to a completely fresh state.
    // This avoids carrying over authentication/lockout UI state from the
    // previous session.
    if (m_lockoutTimer->isActive()) {
        m_lockoutTimer->stop();
    }

    m_failedAttempts = 0;
    m_lockoutRemainingSeconds = 0;
    m_state = LoginState::Ready;

    ui->usernameLineEdit->clear();
    ui->passwordLineEdit->clear();
    ui->passwordLineEdit->setEchoMode(QLineEdit::Password);
    ui->togglePasswordButton->setText(QStringLiteral("显示"));

    ui->lockoutLabel->clear();
    clearPasswordFeedback();
    setLoginEnabled(true);
    ui->loginButton->setText(QStringLiteral("登 录"));
    setStatusMessage(QString(), false);

    ui->usernameLineEdit->setFocus();
}

void AdminLoginWindow::setStatusMessage(
    const QString &message,
    bool error)
{
    ui->statusLabel->setText(message);
    ui->statusLabel->setProperty("error", error);

    // The QLabel stylesheet uses the dynamic "error" property.
    ui->statusLabel->style()->unpolish(ui->statusLabel);
    ui->statusLabel->style()->polish(ui->statusLabel);
    ui->statusLabel->update();
}

void AdminLoginWindow::setLoginEnabled(bool enabled)
{
    ui->usernameLineEdit->setEnabled(enabled);
    ui->passwordLineEdit->setEnabled(enabled);
    ui->togglePasswordButton->setEnabled(enabled);
    ui->loginButton->setEnabled(enabled);
}

void AdminLoginWindow::clearPasswordFeedback()
{
    ui->passwordHintLabel->clear();
}

bool AdminLoginWindow::validateInput(
    QString &username,
    QString &password)
{
    username = ui->usernameLineEdit->text().trimmed();
    password = ui->passwordLineEdit->text();

    if (username.isEmpty()) {
        setStatusMessage(
            QStringLiteral("请输入管理员账号"),
            true
        );
        ui->usernameLineEdit->setFocus();
        return false;
    }

    if (password.isEmpty()) {
        setStatusMessage(
            QStringLiteral("请输入管理员密码"),
            true
        );
        ui->passwordLineEdit->setFocus();
        return false;
    }

    return true;
}

void AdminLoginWindow::onLoginClicked()
{
    if (m_state == LoginState::Locked ||
        m_state == LoginState::Authenticating) {
        return;
    }

    QString username;
    QString password;

    if (!validateInput(username, password)) {
        return;
    }

    m_state = LoginState::Authenticating;
    setLoginEnabled(false);
    ui->loginButton->setText(QStringLiteral("正在验证..."));

    ui->statusLabel->setProperty("error", false);
    ui->statusLabel->style()->unpolish(ui->statusLabel);
    ui->statusLabel->style()->polish(ui->statusLabel);

    AdminAuthService::AdminInfo admin;
    QString errorMessage;

    const bool success = m_authService.authenticate(
        username,
        password,
        admin,
        errorMessage
    );

    if (success) {
        m_failedAttempts = 0;
        m_state = LoginState::Ready;

        setStatusMessage(
            QStringLiteral("登录成功，正在进入管理后台..."),
            false
        );

        emit loginSucceeded(admin);
        hide();

        return;
    }

    m_state = LoginState::Ready;
    ++m_failedAttempts;

    // Do not expose whether the username exists.
    if (errorMessage.startsWith(
            QStringLiteral("管理员验证失败："))) {
        setStatusMessage(errorMessage, true);
    } else {
        setStatusMessage(
            QStringLiteral("账号或密码错误"),
            true
        );
    }

    ui->passwordLineEdit->clear();
    ui->passwordLineEdit->setFocus();
    ui->loginButton->setText(QStringLiteral("登 录"));
    setLoginEnabled(true);

    if (m_failedAttempts >= 5) {
        enterLockout();
    }
}

void AdminLoginWindow::onTogglePasswordClicked()
{
    const bool hidden =
        ui->passwordLineEdit->echoMode() ==
        QLineEdit::Password;

    ui->passwordLineEdit->setEchoMode(
        hidden
            ? QLineEdit::Normal
            : QLineEdit::Password
    );

    ui->togglePasswordButton->setText(
        hidden
            ? QStringLiteral("隐藏")
            : QStringLiteral("显示")
    );

    ui->passwordLineEdit->setFocus();
    ui->passwordLineEdit->setCursorPosition(
        ui->passwordLineEdit->text().size()
    );
}

void AdminLoginWindow::enterLockout()
{
    m_state = LoginState::Locked;
    m_lockoutRemainingSeconds = 30;

    setLoginEnabled(false);
    ui->loginButton->setText(QStringLiteral("已锁定 · 30 秒"));
    ui->lockoutLabel->setText(
        QStringLiteral("连续登录失败 5 次，输入已暂时锁定")
    );

    setStatusMessage(
        QStringLiteral("为保护账户安全，请 30 秒后重试"),
        true
    );

    m_lockoutTimer->start();
}

void AdminLoginWindow::updateLockoutCountdown()
{
    --m_lockoutRemainingSeconds;

    if (m_lockoutRemainingSeconds <= 0) {
        leaveLockout();
        return;
    }

    ui->loginButton->setText(
        QStringLiteral("已锁定 · %1 秒")
            .arg(m_lockoutRemainingSeconds)
    );

    ui->lockoutLabel->setText(
        QStringLiteral(
            "连续登录失败 5 次 · %1 秒后可再次尝试"
        ).arg(m_lockoutRemainingSeconds)
    );
}

void AdminLoginWindow::leaveLockout()
{
    m_lockoutTimer->stop();

    m_state = LoginState::Ready;
    m_failedAttempts = 0;

    setLoginEnabled(true);

    ui->loginButton->setText(QStringLiteral("登 录"));
    ui->lockoutLabel->clear();

    setStatusMessage(
        QStringLiteral("现在可以重新登录"),
        false
    );

    ui->passwordLineEdit->clear();
    ui->usernameLineEdit->setFocus();
}
