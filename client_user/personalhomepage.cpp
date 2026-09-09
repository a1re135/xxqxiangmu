#include "personalhomepage.h"
#include "ui_personalhomepage.h"
#include "loginwindow.h"
#include "ordershistorydialog.h"
#include "albumpickerdialog.h"
#if NCS_HAS_CAMERA
#include "avatarcapturedialog.h"
#endif

#include <QFileDialog>
#include <QPixmap>
#include <QMessageBox>
#include <QDoubleValidator>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QColor>
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMenu>
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <QFile>

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

void showCustomerServiceDialog(
    QWidget *parent)
{
    QDialog dialog(parent);

    dialog.setWindowTitle(
        QStringLiteral("联系客服")
    );

    dialog.setFixedSize(
        360,
        250
    );

    dialog.setStyleSheet(R"(

        QDialog {
            background-color: #08111F;
        }

        QLabel {
            background: transparent;
            color: #DCEBFF;
        }

        QLabel#serviceIcon {
            background-color: #163B63;

            color: #60A5FA;

            border-radius: 25px;

            font-size: 24px;
        }

        QLabel#serviceTitle {
            color: #FFFFFF;

            font-size: 20px;
            font-weight: 800;
        }

        QLabel#serviceSubtitle {
            color: #8199B8;

            font-size: 11px;
        }

        QFrame#serviceCard {
            background-color: #0E2037;

            border: 1px solid #295078;
            border-radius: 13px;
        }

        QLabel#phoneTitle {
            color: #8FA9C8;

            font-size: 11px;
        }

        QLabel#phoneValue {
            color: #60A5FA;

            font-size: 19px;
            font-weight: 800;
        }

        QPushButton {
            background-color: #2563EB;

            color: white;

            border: 1px solid #4B8CFF;
            border-radius: 9px;

            min-height: 38px;

            font-size: 13px;
            font-weight: 700;
        }

        QPushButton:hover {
            background-color: #397EF1;
        }

    )");


    auto *layout =
        new QVBoxLayout(&dialog);

    layout->setContentsMargins(
        20, 18, 20, 18
    );

    layout->setSpacing(10);


    auto *icon =
        new QLabel(
            QStringLiteral("🎧"),
            &dialog
        );

    icon->setObjectName(
        QStringLiteral("serviceIcon")
    );

    icon->setFixedSize(
        50,
        50
    );

    icon->setAlignment(
        Qt::AlignCenter
    );


    auto *title =
        new QLabel(
            QStringLiteral(
                "NCS 客户服务"
            ),
            &dialog
        );

    title->setObjectName(
        QStringLiteral("serviceTitle")
    );


    auto *subtitle =
        new QLabel(
            QStringLiteral(
                "如有充电或订单问题，"
                "请联系我们"
            ),
            &dialog
        );

    subtitle->setObjectName(
        QStringLiteral(
            "serviceSubtitle"
        )
    );


    auto *header =
        new QHBoxLayout;

    header->addWidget(icon);

    auto *headerText =
        new QVBoxLayout;

    headerText->addWidget(title);
    headerText->addWidget(subtitle);

    header->addLayout(
        headerText,
        1
    );


    layout->addLayout(header);


    auto *card =
        new QFrame(&dialog);

    card->setObjectName(
        QStringLiteral("serviceCard")
    );


    auto *cardLayout =
        new QVBoxLayout(card);

    cardLayout->setContentsMargins(
        16, 13, 16, 13
    );


    auto *phoneTitle =
        new QLabel(
            QStringLiteral(
                "客服电话"
            ),
            card
        );

    phoneTitle->setObjectName(
        QStringLiteral(
            "phoneTitle"
        )
    );


    auto *phone =
        new QLabel(
            QStringLiteral(
                "400-000-0000"
            ),
            card
        );

    phone->setObjectName(
        QStringLiteral(
            "phoneValue"
        )
    );


    cardLayout->addWidget(
        phoneTitle
    );

    cardLayout->addWidget(
        phone
    );


    layout->addWidget(card);


    auto *okButton =
        new QPushButton(
            QStringLiteral("我知道了"),
            &dialog
        );

    layout->addWidget(
        okButton
    );

    QObject::connect(
        okButton,
        &QPushButton::clicked,
        &dialog,
        &QDialog::accept
    );


    dialog.exec();
}

}

PersonalHomePage::PersonalHomePage(UserService &userService, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::PersonalHomePage), m_userService(userService)
{
    ui->setupUi(this);
    setFixedSize(420, 760); // .ui 中已设置 min/max size，这里再保险一层

    setStyleSheet(
        "QMainWindow {"
        "background-color:#08111F;"
        "}"
    );

    ui->profileCard->setStyleSheet(
        QStringLiteral(
            "QFrame#profileCard {"
            "background:#0E1D32;"
            "border:1px solid #243E5E;"
            "border-radius:16px;"
            "}"
        )
    );

    ui->walletCard->setStyleSheet(
        QStringLiteral(
            "QFrame#walletCard {"
            "background:#0E1D32;"
            "border:1px solid #243E5E;"
            "border-radius:16px;"
            "}"
        )
    );

    ui->quickActionsCard->setStyleSheet(
        QStringLiteral(
            "QFrame#quickActionsCard {"
            "background:#0E1D32;"
            "border:1px solid #243E5E;"
            "border-radius:16px;"
            "}"
        )
    );


    // Remove the strange dark bars behind section titles.
    ui->walletTitleLabel->setStyleSheet(
        "background:transparent;"
        "color:#8FA9C8;"
        "font-size:12px;"
        "font-weight:600;"
    );

    ui->quickTitleLabel->setStyleSheet(
        "background:transparent;"
        "color:#8FA9C8;"
        "font-size:12px;"
        "font-weight:600;"
    );


    ui->balanceValueLabel->setStyleSheet(
        "background:transparent;"
        "color:#FFFFFF;"
        "font-size:30px;"
        "font-weight:800;"
    );


    ui->nicknameEdit->setStyleSheet(R"(

        QLineEdit {
            background-color: #091729;

            color: #F2F5FA;

            border: 1px solid #294766;
            border-radius: 10px;

            padding: 9px 11px;
        }

        QLineEdit:focus {
            border: 1px solid #60A5FA;
        }

    )");


    ui->rechargeEdit->setStyleSheet(
        ui->nicknameEdit->styleSheet()
    );


    const QString primaryButtonStyle = R"(

        QPushButton {
            background-color: #2563EB;

            color: white;

            border: 1px solid #4B8CFF;
            border-radius: 9px;

            padding: 9px 15px;

            font-weight: 700;
        }

        QPushButton:hover {
            background-color: #397EF1;
        }

    )";


    ui->saveNicknameBtn->setStyleSheet(
        primaryButtonStyle
    );

    ui->rechargeBtn->setStyleSheet(
        primaryButtonStyle
    );


    const QString actionButtonStyle = R"(

        QPushButton {
            background-color: #10233B;

            color: #D8E9FF;

            border: 1px solid #294B6D;
            border-radius: 11px;

            font-size: 12px;
            font-weight: 600;
        }

        QPushButton:hover {
            background-color: #173655;

            color: #FFFFFF;

            border: 1px solid #60A5FA;
        }

    )";


    ui->homeBtn->setStyleSheet(
        actionButtonStyle
    );

    ui->ordersBtn->setStyleSheet(
        actionButtonStyle
    );

    ui->serviceBtn->setStyleSheet(
        actionButtonStyle
    );


    ui->logoutBtn->setStyleSheet(R"(

        QPushButton {
            background-color: #10233B;

            color: #FB7185;

            border: 1px solid #71394F;
            border-radius: 11px;

            font-size: 12px;
            font-weight: 600;
        }

        QPushButton:hover {
            background-color: #39202C;

            border-color: #FB7185;
        }

    )");
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

    connect(
        ui->serviceBtn,
        &QPushButton::clicked,
        this,
        [this]()
        {
            showCustomerServiceDialog(this);
        }
    );

    connect(
        ui->ordersBtn,
        &QPushButton::clicked,
        this,
        [this]()
        {
            if (m_user.id <= 0) {
                return;
            }

            OrderHistoryDialog dialog(
                m_user.id,
                this
            );

            dialog.exec();
        }
    );
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
    // 点击头像：拍照 / 从相册选择 / 从文件选择。
    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(R"(
QMenu {
    background: #14243C;
    border: 1px solid #264B70;
    border-radius: 8px;
    padding: 6px;
}
QMenu::item {
    color: #E5EFFF;
    padding: 10px 22px;
    border-radius: 6px;
    font-size: 13px;
}
QMenu::item:selected {
    background: #2563EB;
    color: white;
}
)"));
    menu.setMinimumWidth(180);

#if NCS_HAS_CAMERA
    QAction *cameraAction = menu.addAction(QStringLiteral("📷 拍照上传"));
#endif
    QAction *albumAction = menu.addAction(QStringLiteral("🖼 从相册选择"));
    QAction *fileAction = menu.addAction(QStringLiteral("📁 从文件选择"));

    QAction *chosen = menu.exec(
        ui->avatarLabel->mapToGlobal(
            QPoint(0, ui->avatarLabel->height() + 4)));
    if (!chosen)
        return;

#if NCS_HAS_CAMERA
    if (chosen == cameraAction) {
        applyAvatarFromCamera();
        return;
    }
#endif

    if (chosen == albumAction) {
        AlbumPickerDialog dialog(this);
        if (dialog.exec() != QDialog::Accepted)
            return;

        const QString path = dialog.selectedFilePath();
        if (path.isEmpty())
            return;

        QString err;
        if (!m_userService.updateAvatar(m_user.id, path, err)) {
            QMessageBox::warning(this, "更新失败", err);
            return;
        }
        reloadFromService();
        return;
    }

    if (chosen == fileAction) {
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
}

#if NCS_HAS_CAMERA
void PersonalHomePage::applyAvatarFromCamera()
{
    AvatarCaptureDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QString source = dialog.capturedFilePath();
    if (source.isEmpty())
        return;

    // 拍照结果落在系统临时目录，复制到应用数据目录 avatars/ 下，
    // 避免临时文件被系统清理导致头像丢失。
    const QString destDir = QStandardPaths::writableLocation(
                                QStandardPaths::AppDataLocation)
                            + QStringLiteral("/avatars");
    QDir().mkpath(destDir);

    const QString dest = destDir
                         + QStringLiteral("/u%1_%2.jpg")
                               .arg(m_user.id)
                               .arg(QDateTime::currentMSecsSinceEpoch());

    if (QFile::copy(source, dest))
        source = dest;

    QString err;
    if (!m_userService.updateAvatar(m_user.id, source, err)) {
        QMessageBox::warning(this, "更新失败", err);
        return;
    }
    reloadFromService();
}
#endif

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
