#include "personalhomepage.h"
#include "ui_personalhomepage.h"
#include "loginwindow.h"

#include <QFileDialog>
#include <QPixmap>
#include <QMessageBox>
#include <QDoubleValidator>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QColor>

namespace {
const char *kSuccess = "#22C55E";
const char *kDanger  = "#EF4444";

QString maskPhone(const QString &phone)
{
    if (phone.size() != 11) return phone;
    return phone.left(3) + "****" + phone.right(4);
}

QGraphicsDropShadowEffect *makeShadow()
{
    auto *shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 120));
    return shadow;
}
}

PersonalHomePage::PersonalHomePage(UserService &userService, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::PersonalHomePage), m_userService(userService)
{
    ui->setupUi(this);
    setFixedSize(420, 760); // .ui 中已设置 min/max size，这里再保险一层

    applyCardShadows();

    ui->rechargeEdit->setValidator(new QDoubleValidator(0.01, 100000, 2, this));

    ui->avatarLabel->installEventFilter(this);

    connect(ui->saveNicknameBtn, &QPushButton::clicked, this, &PersonalHomePage::onSaveNicknameClicked);
    connect(ui->rechargeBtn, &QPushButton::clicked, this, &PersonalHomePage::onRechargeClicked);
    connect(ui->homeBtn, &QPushButton::clicked, this, &PersonalHomePage::backToHomeRequested);
    connect(ui->logoutBtn, &QPushButton::clicked, this, &PersonalHomePage::onlogoutBtnClicked);
    // FIX: 原 .ui 将这两个按钮 disabled，所以点击没有任何反应。
    ui->ordersBtn->setEnabled(true);
    ui->serviceBtn->setEnabled(true);
    ui->ordersBtn->setCursor(Qt::PointingHandCursor);
    ui->serviceBtn->setCursor(Qt::PointingHandCursor);

    connect(ui->ordersBtn, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(this, "我的订单", "订单模块还未接入，目前按钮已经可以正常响应。");
    });
    connect(ui->serviceBtn, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(this, "联系客服", "客服电话：400-000-0000\n（当前为演示功能）");
    });
}

PersonalHomePage::~PersonalHomePage()
{
    delete ui;
}

void PersonalHomePage::applyCardShadows()
{
    // Qt Designer 的属性编辑器不方便直接配置 QGraphicsEffect，这里在 setupUi 之后手动补上投影
    ui->profileCard->setGraphicsEffect(makeShadow());
    ui->walletCard->setGraphicsEffect(makeShadow());
    ui->quickActionsCard->setGraphicsEffect(makeShadow());
}

bool PersonalHomePage::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->avatarLabel && event->type() == QEvent::MouseButtonRelease) {
        onChangeAvatarClicked();
        return true;
    }
    return QMainWindow::eventFilter(watched, event);
}

void PersonalHomePage::setUser(const UserInfo &user)
{
    m_user = user;
    refreshDisplay();
}

void PersonalHomePage::reloadFromService()
{
    UserInfo latest;
    QString err;
    if (m_userService.getUserById(m_user.id, latest, err)) {
        m_user = latest;
        refreshDisplay();
    } else if (!err.isEmpty()) {
        QMessageBox::warning(this, "刷新失败", err);
    }
}

void PersonalHomePage::refreshDisplay()
{
    ui->heroTitleLabel->setText(QString("Hi, %1 👋").arg(m_user.nickname));
    ui->heroSubtitleLabel->setText(QString("手机号 %1").arg(maskPhone(m_user.phone)));

    ui->nicknameEdit->setText(m_user.nickname);
    ui->phoneLabel->setText(maskPhone(m_user.phone));
    ui->balanceValueLabel->setText(QString("¥ %1").arg(m_user.balance, 0, 'f', 2));

    if (!m_user.avatarPath.isEmpty()) {
        QPixmap pix(m_user.avatarPath);
        if (!pix.isNull()) {
            ui->avatarLabel->setText("");
            ui->avatarLabel->setPixmap(pix);
        }
    } else {
        ui->avatarLabel->setPixmap(QPixmap());
        ui->avatarLabel->setText("👤");
    }

    refreshStatusBadge();
}

void PersonalHomePage::refreshStatusBadge()
{
    if (m_user.status == 0) {
        ui->statusBadgeLabel->setText("● 账号已冻结");
        ui->statusBadgeLabel->setStyleSheet(QString(
            "background: rgba(239,68,68,0.15); color:%1; border-radius:10px; padding:4px 10px; font-size:11px;")
            .arg(kDanger));
    } else {
        ui->statusBadgeLabel->setText("● 账号正常");
        ui->statusBadgeLabel->setStyleSheet(QString(
            "background: rgba(34,197,94,0.15); color:%1; border-radius:10px; padding:4px 10px; font-size:11px;")
            .arg(kSuccess));
    }
}

void PersonalHomePage::onChangeAvatarClicked()
{
    const QString path = QFileDialog::getOpenFileName(this, "选择头像图片", QString(),
                                                        "图片文件 (*.png *.jpg *.jpeg *.bmp)");
    if (path.isEmpty()) return;

    QString err;
    if (!m_userService.updateAvatar(m_user.id, path, err)) {
        QMessageBox::warning(this, "更新失败", err);
        return;
    }
    reloadFromService();
}

void PersonalHomePage::onSaveNicknameClicked()
{
    const QString nickname = ui->nicknameEdit->text().trimmed();
    if (nickname.isEmpty()) {
        ui->profileHintLabel->setStyleSheet(QString("color:%1; font-size:11px;").arg(kDanger));
        ui->profileHintLabel->setText("昵称不能为空");
        return;
    }
    QString err;
    if (!m_userService.updateNickname(m_user.id, nickname, err)) {
        ui->profileHintLabel->setStyleSheet(QString("color:%1; font-size:11px;").arg(kDanger));
        ui->profileHintLabel->setText(err);
        return;
    }
    reloadFromService();
    ui->profileHintLabel->setStyleSheet(QString("color:%1; font-size:11px;").arg(kSuccess));
    ui->profileHintLabel->setText("昵称已更新");
}

void PersonalHomePage::onRechargeClicked()
{
    bool ok = false;
    const double amount = ui->rechargeEdit->text().toDouble(&ok);
    if (!ok || amount <= 0) {
        ui->walletHintLabel->setStyleSheet(QString("color:%1; font-size:11px;").arg(kDanger));
        ui->walletHintLabel->setText("请输入正确的充值金额");
        return;
    }

    double newBalance = 0.0;
    QString err;
    if (!m_userService.recharge(m_user.id, amount, newBalance, err)) {
        ui->walletHintLabel->setStyleSheet(QString("color:%1; font-size:11px;").arg(kDanger));
        ui->walletHintLabel->setText(err);
        return;
    }
    ui->rechargeEdit->clear();
    // FIX: 先使用事务返回的新余额立即刷新，再从数据库重新读取一次校验。
    m_user.balance = newBalance;
    refreshDisplay();
    reloadFromService();
    ui->walletHintLabel->setStyleSheet(QString("color:%1; font-size:11px;").arg(kSuccess));
    ui->walletHintLabel->setText(QString("模拟支付成功，当前余额 ¥%1").arg(newBalance, 0, 'f', 2));
}
void PersonalHomePage::onlogoutBtnClicked()
{
    emit logoutRequested();
}
