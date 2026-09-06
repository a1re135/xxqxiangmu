#include "loginwindow.h"
#include "./ui_loginwindow.h"

#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStyle>

LoginWindow::LoginWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::LoginWindow)
    , m_countdownTimer(new QTimer(this))
    , m_remainingSeconds(0)
{
    ui->setupUi(this);

    setFixedSize(420, 760);

    m_countdownTimer->setInterval(1000);

    ui->statusLabel->clear();
    ui->verificationInfoLabel->clear();
    ui->countdownLabel->clear();

    connect(
        ui->getCodeButton,
        &QPushButton::clicked,
        this,
        &LoginWindow::onGetCodeClicked
    );

    connect(
        ui->loginButton,
        &QPushButton::clicked,
        this,
        &LoginWindow::onLoginClicked
    );

    connect(
        m_countdownTimer,
        &QTimer::timeout,
        this,
        &LoginWindow::updateCountdown
    );

    // Pressing Enter in the verification field is equivalent to login.
    connect(
        ui->verificationCodeLineEdit,
        &QLineEdit::returnPressed,
        this,
        &LoginWindow::onLoginClicked
    );
}

LoginWindow::~LoginWindow()
{
    delete ui;
}

bool LoginWindow::validatePhone(QString &phone)
{
    phone = ui->phoneLineEdit->text().trimmed();

    const QRegularExpression regex(QStringLiteral("^1[0-9]{10}$"));

    if (!regex.match(phone).hasMatch()) {
        setStatusMessage(QStringLiteral("请输入正确的11位手机号"), true);
        ui->phoneLineEdit->setFocus();
        return false;
    }

    return true;
}

void LoginWindow::setStatusMessage(const QString &message, bool error)
{
    ui->statusLabel->setText(message);

    ui->statusLabel->setProperty("error", error);
    ui->statusLabel->style()->unpolish(ui->statusLabel);
    ui->statusLabel->style()->polish(ui->statusLabel);
    ui->statusLabel->update();
}

void LoginWindow::resetVerificationState()
{
    m_countdownTimer->stop();
    m_currentVerificationCode.clear();
    m_remainingSeconds = 0;

    ui->getCodeButton->setEnabled(true);
    ui->getCodeButton->setText(QStringLiteral("获取验证码"));
    ui->countdownLabel->clear();
    ui->verificationInfoLabel->clear();
}

void LoginWindow::onGetCodeClicked()
{
    QString phone;

    if (!validatePhone(phone)) {
        return;
    }

    Q_UNUSED(phone);

    // Six-digit simulated SMS code.
    m_currentVerificationCode = QString::number(
        QRandomGenerator::global()->bounded(100000, 1000000)
    );

    ui->verificationInfoLabel->setText(
        QStringLiteral("模拟验证码  ·  ") + m_currentVerificationCode
    );

    setStatusMessage(
        QStringLiteral("验证码已发送，已在界面显示（模拟）"),
        false
    );

    m_remainingSeconds = 60;

    ui->getCodeButton->setEnabled(false);
    ui->getCodeButton->setText(QStringLiteral("重新获取"));

    ui->countdownLabel->setText(
        QStringLiteral("验证码有效期  %1 秒").arg(m_remainingSeconds)
    );

    m_countdownTimer->start();

    ui->verificationCodeLineEdit->clear();
    ui->verificationCodeLineEdit->setFocus();
}

void LoginWindow::updateCountdown()
{
    --m_remainingSeconds;

    if (m_remainingSeconds <= 0) {
        m_countdownTimer->stop();

        ui->getCodeButton->setEnabled(true);
        ui->getCodeButton->setText(QStringLiteral("重新获取"));
        ui->countdownLabel->setText(
            QStringLiteral("验证码已过期，请重新获取")
        );

        m_currentVerificationCode.clear();
        return;
    }

    ui->countdownLabel->setText(
        QStringLiteral("验证码有效期  %1 秒").arg(m_remainingSeconds)
    );
}

void LoginWindow::onLoginClicked()
{
    QString phone;

    if (!validatePhone(phone)) {
        return;
    }

    if (m_currentVerificationCode.isEmpty()) {
        setStatusMessage(QStringLiteral("请先获取验证码"), true);
        return;
    }

    const QString verificationCode =
        ui->verificationCodeLineEdit->text().trimmed();

    if (verificationCode.isEmpty()) {
        setStatusMessage(QStringLiteral("请输入验证码"), true);
        ui->verificationCodeLineEdit->setFocus();
        return;
    }

    UserInfo userInfo;
    QString errorMessage;

    if (!m_userService.loginOrRegister(
            phone,
            verificationCode,
            m_currentVerificationCode,
            userInfo,
            errorMessage)) {
        setStatusMessage(errorMessage, true);
        return;
    }

    resetVerificationState();
    setStatusMessage(QStringLiteral("登录成功"), false);

    // FIX: LoginWindow 只负责登录并发出信号，不再自己创建 PersonalHomePage。
    // 窗口切换统一交给 main.cpp 管理，避免登录后同时出现 MainWindow 和个人主页两套流程。
    emit loginSucceeded(userInfo);
}

UserService &LoginWindow::userService()
{
    return m_userService;
}

