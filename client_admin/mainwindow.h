#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "service/adminauthservice.h"

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

    void setCurrentAdmin(
        const AdminAuthService::AdminInfo &admin
    );

private:
    Ui::MainWindow *ui;

    AdminAuthService::AdminInfo m_currentAdmin;
};

#endif // MAINWINDOW_H
