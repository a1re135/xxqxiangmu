#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

namespace core {
class StationService;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(core::StationService *stationService,
                        QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    core::StationService *m_stationService;
};

#endif // MAINWINDOW_H
