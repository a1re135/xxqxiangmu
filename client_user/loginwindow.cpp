#include "loginwindow.h"

#include "./ui_loginwindow.h"

#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStyle>

#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QShowEvent>
#include <QLineEdit>
#include <QColor>

LoginWindow::LoginWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::LoginWindow)
    , m_countdownTimer(new QTimer(this))
    , m_remainingSeconds(0)
    , m_verificationOpacityEffect(nullptr)
{
    ui->setupUi(this);

    // Keep the login screen exactly 420 x 760, matching the requested design.
    setFixedSize(420, 760);

    m_countdownTimer->setInterval(1000);
    ui->statusLabel->clear();
    ui->verificationInfoLabel->clear();
    ui->countdownLabel->clear();

    // --- visual polish: elevation + focus glow ---
    applyCardElevation();
    installFocusGlow(ui->phoneLineEdit);
    installFocusGlow(ui->verificationCodeLineEdit);

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

void LoginWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);

    // Only play the entrance animation for a real, code-triggered show —
    // not for spontaneous window-system events (e.g. un-minimizing).
    if (event->spontaneous()) {
        return;
    }

    auto *fade = new QPropertyAnimation(this, "windowOpacity", this);
    fade->setDuration(360);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);
    fade->start(QAbstractAnimation::DeleteWhenStopped);

    const QPoint cardRestingPos = ui->loginCard->pos();
    ui->loginCard->move(cardRestingPos.x(), cardRestingPos.y() + 18);

    auto *slideUp = new QPropertyAnimation(ui->loginCard, "pos", this);
    slideUp->setDuration(420);
    slideUp->setStartValue(ui->loginCard->pos());
    slideUp->setEndValue(cardRestingPos);
    slideUp->setEasingCurve(QEasingCurve::OutCubic);
    slideUp->start(QAbstractAnimation::DeleteWhenStopped);
}

bool LoginWindow::eventFilter(QObject *watched, QEvent *event)
{
    auto *lineEdit = qobject_cast<QLineEdit *>(watched);

    if (lineEdit
        && (lineEdit == ui->phoneLineEdit
            || lineEdit == ui->verificationCodeLineEdit)) {
        if (event->type() == QEvent::FocusIn) {
            auto *glow = new QGraphicsDropShadowEffect(lineEdit);
            glow->setBlurRadius(24);
            glow->setOffset(0, 0);
            glow->setColor(QColor(192, 132, 252, 180));
            lineEdit->setGraphicsEffect(glow);
        } else if (event->type() == QEvent::FocusOut) {
            // Passing nullptr removes (and deletes) the installed effect.
            lineEdit->setGraphicsEffect(nullptr);
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void LoginWindow::applyCardElevation()
{
    auto *cardShadow = new QGraphicsDropShadowEffect(ui->loginCard);
    cardShadow->setBlurRadius(40);
    cardShadow->setOffset(0, 16);
    cardShadow->setColor(QColor(10, 4, 24, 200));
    ui->loginCard->setGraphicsEffect(cardShadow);

    // Warm violet-to-pink glow beneath the CTA button, echoing its gradient.
    auto *buttonGlow = new QGraphicsDropShadowEffect(ui->loginButton);
    buttonGlow->setBlurRadius(32);
    buttonGlow->setOffset(0, 8);
    buttonGlow->setColor(QColor(219, 39, 119, 150));
    ui->loginButton->setGraphicsEffect(buttonGlow);
}

void LoginWindow::installFocusGlow(QLineEdit *lineEdit)
{
    if (!lineEdit) {
        return;
    }

    lineEdit->installEventFilter(this);
}

void LoginWindow::playShakeAnimation(QWidget *target)
{
    if (!target) {
        return;
    }

    const QPoint origin = target->pos();

    auto *shake = new QPropertyAnimation(target, "pos", target);
    shake->setDuration(320);
    shake->setKeyValueAt(0.0, origin);
    shake->setKeyValueAt(0.15, origin + QPoint(-8, 0));
    shake->setKeyValueAt(0.35, origin + QPoint(7, 0));
    shake->setKeyValueAt(0.55, origin + QPoint(-5, 0));
    shake->setKeyValueAt(0.75, origin + QPoint(3, 0));
    shake->setKeyValueAt(1.0, origin);
    shake->setEasingCurve(QEasingCurve::InOutQuad);
    shake->start(QAbstractAnimation::DeleteWhenStopped);
}

void LoginWindow::animateVerificationReveal()
{
    if (!m_verificationOpacityEffect) {
        m_verificationOpacityEffect = new QGraphicsOpacityEffect(ui->verificationInfoLabel);
        ui->verificationInfoLabel->setGraphicsEffect(m_verificationOpacityEffect);
    }

    auto *fade = new QPropertyAnimation(m_verificationOpacityEffect, "opacity", this);
    fade->setDuration(320);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);
    fade->start(QAbstractAnimation::DeleteWhenStopped);
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

    if (error) {
        playShakeAnimation(ui->loginCard);
    }
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
    animateVerificationReveal();

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
