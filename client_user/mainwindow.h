#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "service/userservice.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void setCurrentUser(const UserInfo &user);

private:
    Ui::MainWindow *ui;

    UserInfo m_currentUser;
};

#endif // MAINWINDOW_H
