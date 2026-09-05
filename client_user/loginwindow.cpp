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

    // Keep the login screen exactly 420 x 760, matching the requested design.
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

    // Small convenience from the reference design: clear the phone number.
    connect(
        ui->clearPhoneButton,
        &QPushButton::clicked,
        this,
        [this]() {
            ui->phoneLineEdit->clear();
            ui->phoneLineEdit->setFocus();
            resetVerificationState();
            ui->statusLabel->clear();
        }
    );

    connect(
        ui->phoneLineEdit,
        &QLineEdit::textChanged,
        this,
        [this]() {
            if (!m_currentVerificationCode.isEmpty()) {
                resetVerificationState();
                setStatusMessage(QStringLiteral("手机号已更改，请重新获取验证码"), false);
            }
        }
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

    m_currentVerificationCode = QString::number(
        QRandomGenerator::global()->bounded(100000, 1000000)
    );

    ui->verificationInfoLabel->setText(
        QStringLiteral("模拟验证码: ") + m_currentVerificationCode
    );

    setStatusMessage(
        QStringLiteral("验证码已生成，请在60秒内输入"),
        false
    );

    m_remainingSeconds = 60;
    ui->getCodeButton->setEnabled(false);
    ui->countdownLabel->setText(
        QStringLiteral("有效期 %1 秒").arg(m_remainingSeconds)
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
        ui->countdownLabel->setText(QStringLiteral("验证码已过期"));
        ui->verificationInfoLabel->clear();
        m_currentVerificationCode.clear();
        return;
    }
    ui->countdownLabel->setText(
        QStringLiteral("有效期 %1 秒").arg(m_remainingSeconds)
    );
}

void LoginWindow::onLoginClicked()
{
    QString phone;

    // BR-01: validate before touching the database.
    if (!validatePhone(phone)) {
        return;
    }

    if (!ui->userAgreementCheckBox->isChecked()) {
        setStatusMessage(QStringLiteral("请先阅读并同意用户服务协议与隐私政策"), true);
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

    const bool success = m_userService.loginOrRegister(
        phone,
        verificationCode,
        m_currentVerificationCode,
        userInfo,
        errorMessage
    );

    if (!success) {
        setStatusMessage(errorMessage, true);
        return;
    }

    resetVerificationState();
    emit loginSucceeded(userInfo);
    hide();
}
