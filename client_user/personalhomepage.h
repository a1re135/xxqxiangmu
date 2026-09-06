#ifndef PERSONALHOMEPAGE_H
#define PERSONALHOMEPAGE_H

#include <QMainWindow>
#include "service/userservice.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class PersonalHomePage;
}
QT_END_NAMESPACE

// 个人主页：头像/昵称维护、账号状态徽标（对接 BR-07 冻结状态）、钱包余额与充值。
class PersonalHomePage : public QMainWindow
{
    Q_OBJECT
public:
    explicit PersonalHomePage(UserService &userService, QWidget *parent = nullptr);
    ~PersonalHomePage();

signals:
    void logoutRequested();          // 点击“退出登录”
    void backToHomeRequested();      // 点击“返回充电站列表”

public slots:
    // 与 LoginWindow::loginSucceeded(const UserInfo&) 直接相连即可加载当前用户
    void setUser(const UserInfo &user);
    void reloadFromService();        // 充值/改资料后从 UserService 重新拉取最新数据

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onChangeAvatarClicked();
    void onSaveNicknameClicked();
    void onRechargeClicked();
    void onlogoutBtnClicked();

private:
    void applyCardShadows();
    void refreshStatusBadge();
    void refreshDisplay();

    Ui::PersonalHomePage *ui;
    UserService &m_userService;
    UserInfo m_user;
};

#endif // PERSONALHOMEPAGE_H
