#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "mainwindowhelpers.h"

#include <algorithm>
#include <QDate>
#include <QDateTime>
#include <QFont>
#include <QInputDialog>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QSignalBlocker>
#include <QSqlDatabase>
#include <QTime>
#include <QFormLayout>
#include <QGroupBox>

#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QChart>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QAbstractItemView>
#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLayout>
#include <QLayoutItem>
#include <QLineEdit>
#include <QMessageBox>
#include <QModelIndex>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

void MainWindow::restartSelectedCharger()
{
    const int id = selectedChargerId();

    if (id <= 0) {
        QMessageBox::information(
            this,
            QStringLiteral("远程重启"),
            QStringLiteral("请先选择一个电桩。")
        );
        return;
    }

    const int status = selectedChargerStatus();

    QString message;
    if (status == 1) {
        message = QStringLiteral(
            "该电桩正在充电，重启会中断用户充电。\n\n确定要继续吗？"
        );
    } else {
        message = QStringLiteral(
            "确定要远程重启当前电桩吗？"
        );
    }

    QMessageBox confirmBox(this);
    confirmBox.setIcon(QMessageBox::Warning);
    confirmBox.setWindowTitle(QStringLiteral("确认远程重启"));
    confirmBox.setText(message);
    confirmBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    confirmBox.setDefaultButton(QMessageBox::No);

    confirmBox.setStyleSheet(R"(

        QMessageBox {
            background-color: #08111F;
        }

        QMessageBox QLabel {
            color: #EAF3FF;
            font-size: 13px;
        }

        QPushButton {
            min-width: 90px;
            min-height: 34px;
            background-color: #10233B;
            color: #DCEBFF;
            border: 1px solid #315A82;
            border-radius: 8px;
            padding: 6px 12px;
            font-weight: 600;
        }

        QPushButton:hover {
            background-color: #173656;
            border-color: #60A5FA;
        }

    )");

    if (confirmBox.exec() != QMessageBox::Yes) {
        return;
    }

    QProgressDialog progressDialog(
        QStringLiteral("正在向电桩发送重启指令..."),
        QString(),
        0,
        100,
        this
    );

    progressDialog.setWindowTitle(
        QStringLiteral("远程重启")
    );

    progressDialog.setCancelButton(nullptr);

    progressDialog.setWindowModality(
        Qt::ApplicationModal
    );

    progressDialog.setAutoClose(false);
    progressDialog.setAutoReset(false);

    progressDialog.setMinimumDuration(0);

    progressDialog.setStyleSheet(QStringLiteral(R"(

        QProgressDialog {
            background:#08111F;
            color:#EAF3FF;
        }

        QLabel {
            color:#EAF3FF;
            font-size:13px;
            font-weight:600;
        }

        QProgressBar {
            background:#10233B;
            color:#EAF3FF;

            border:1px solid #315A82;
            border-radius:7px;

            text-align:center;
            min-height:22px;
        }

        QProgressBar::chunk {
            background:#10B981;
            border-radius:6px;
        }

    )"));

    progressDialog.setValue(0);
    progressDialog.show();

    auto *restartTimer =
        new QTimer(this);

    restartTimer->setInterval(100);

    auto *elapsedMs =
        new int(0);

    connect(
        restartTimer,
        &QTimer::timeout,
        this,
        [this,
         id,
         restartTimer,
         elapsedMs,
         &progressDialog,
         style = confirmBox.styleSheet()]()
        {
            *elapsedMs += 100;

            const int progress =
                qMin(
                    100,
                    (*elapsedMs * 100) / 2000
                );

            progressDialog.setValue(
                progress
            );


            if (*elapsedMs < 2000) {
                return;
            }


            restartTimer->stop();

            QString errorMessage;

            if (!m_chargerService.restartCharger(
                    id,
                    errorMessage)) {

                progressDialog.close();

                QMessageBox errorBox(this);

                errorBox.setIcon(
                    QMessageBox::Critical
                );

                errorBox.setWindowTitle(
                    QStringLiteral(
                        "远程重启失败"
                    )
                );

                errorBox.setText(
                    errorMessage
                );

                errorBox.setStandardButtons(
                    QMessageBox::Ok
                );

                errorBox.setStyleSheet(
                    style
                );

                errorBox.exec();


                restartTimer->deleteLater();
                delete elapsedMs;

                return;
            }


            progressDialog.setValue(100);
            progressDialog.close();


            refreshChargerManagement();
            refreshChargerStatusOverview();


            QMessageBox okBox(this);

            okBox.setIcon(
                QMessageBox::Information
            );

            okBox.setWindowTitle(
                QStringLiteral(
                    "远程重启成功"
                )
            );

            okBox.setText(
                QStringLiteral(
                    "电桩已完成重启并恢复为空闲状态。"
                )
            );

            okBox.setStandardButtons(
                QMessageBox::Ok
            );

            okBox.setStyleSheet(
                style
            );

            okBox.exec();


            restartTimer->deleteLater();
            delete elapsedMs;
        }
    );

    restartTimer->start();

    progressDialog.exec();

    QString errorMessage;
    if (!m_chargerService.restartCharger(id, errorMessage)) {
        QMessageBox errorBox(this);
        errorBox.setIcon(QMessageBox::Critical);
        errorBox.setWindowTitle(QStringLiteral("远程重启失败"));
        errorBox.setText(errorMessage);
        errorBox.setStandardButtons(QMessageBox::Ok);
        errorBox.setStyleSheet(confirmBox.styleSheet());
        errorBox.exec();
        return;
    }

    refreshChargerManagement();
    refreshChargerStatusOverview();

    QMessageBox okBox(this);
    okBox.setIcon(QMessageBox::Information);
    okBox.setWindowTitle(QStringLiteral("远程重启成功"));
    okBox.setText(QStringLiteral("电桩已恢复为空闲状态。"));
    okBox.setStandardButtons(QMessageBox::Ok);
    okBox.setStyleSheet(confirmBox.styleSheet());
    okBox.exec();
}

void MainWindow::setSelectedChargerInUse()
{
    const int id = selectedChargerId();

    if (id <= 0) {
        QMessageBox::information(
            this,
            QStringLiteral("设为使用中"),
            QStringLiteral(
                "请先选择一个电桩。"
            )
        );

        return;
    }

    const int status =
        selectedChargerStatus();

    if (status != 0) {
        QMessageBox::information(
            this,
            QStringLiteral("设为使用中"),
            QStringLiteral(
                "只有闲置电桩可以设置为使用中。"
            )
        );

        return;
    }

    const auto result =
        QMessageBox::question(
            this,
            QStringLiteral("确认状态变更"),
            QStringLiteral(
                "确定将当前电桩设置为"
                "“使用中”状态吗？\n\n"
                "此操作用于管理员模拟"
                "电桩正在使用。"
            ),
            QMessageBox::Yes |
            QMessageBox::No,
            QMessageBox::No
        );

    if (result != QMessageBox::Yes) {
        return;
    }

    QString errorMessage;

    if (!m_chargerService.setInUse(
            id,
            errorMessage)) {

        QMessageBox::warning(
            this,
            QStringLiteral("设为使用中"),
            errorMessage
        );

        return;
    }

    refreshChargerManagement();
    refreshChargerStatusOverview();

    QMessageBox::information(
        this,
        QStringLiteral("状态已更新"),
        QStringLiteral(
            "电桩已设置为使用中。"
        )
    );
}

void MainWindow::setSelectedChargerFault()
{
    const int id = selectedChargerId();
    if (id <= 0) {
        QMessageBox::information(this, QStringLiteral("标记故障"),
                                 QStringLiteral("请先选择一个电桩。"));
        return;
    }

    if (selectedChargerStatus() == 1) {
        const auto result = QMessageBox::warning(
            this, QStringLiteral("确认标记故障"),
            QStringLiteral("该电桩正在充电，标记故障会中断用户充电。\n确定继续吗？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (result != QMessageBox::Yes) return;
    } else {
        const auto result = QMessageBox::question(
            this, QStringLiteral("确认标记故障"),
            QStringLiteral("确定将该电桩标记为故障吗？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (result != QMessageBox::Yes) return;
    }

    QString errorMessage;
    if (!m_chargerService.setFault(id, errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("标记故障"), errorMessage);
        return;
    }
    refreshChargerManagement();
}


void MainWindow::recoverSelectedCharger()
{
    const int id = selectedChargerId();
    if (id <= 0) {
        QMessageBox::information(this, QStringLiteral("恢复正常"),
                                 QStringLiteral("请先选择一个电桩。"));
        return;
    }

    if (selectedChargerStatus() != 2) {
        QMessageBox::information(this, QStringLiteral("恢复正常"),
                                 QStringLiteral("仅故障电桩可以恢复正常。"));
        return;
    }

    const auto result = QMessageBox::question(
        this,
        QStringLiteral("确认恢复"),
        QStringLiteral("确定将该电桩恢复为闲置状态吗？"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (result != QMessageBox::Yes) return;

    QString errorMessage;
    if (!m_chargerService.recover(id, errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("恢复正常"), errorMessage);
        return;
    }
    refreshChargerManagement();
}

