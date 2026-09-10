#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "mainwindowhelpers.h"
#include "network/chargercommandclient.h"

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
    const int id =
        selectedChargerId();

    if (id <= 0) {

        QMessageBox::information(
            this,
            QStringLiteral("远程重启"),
            QStringLiteral(
                "请先选择一个电桩。"
            )
        );

        return;
    }


    const int status =
        selectedChargerStatus();


    QString message;

    if (status == 1) {

        message =
            QStringLiteral(
                "该电桩正在充电，"
                "重启会中断用户充电。\n\n"
                "确定要继续吗？"
            );

    } else {

        message =
            QStringLiteral(
                "确定要通过 Socket "
                "向当前电桩发送远程重启指令吗？"
            );
    }


    // =========================================================
    // Confirmation
    // =========================================================

    QMessageBox confirmBox(this);

    confirmBox.setIcon(
        QMessageBox::Warning
    );

    confirmBox.setWindowTitle(
        QStringLiteral(
            "确认远程重启"
        )
    );

    confirmBox.setText(
        message
    );

    confirmBox.setStandardButtons(
        QMessageBox::Yes
        | QMessageBox::No
    );

    confirmBox.setDefaultButton(
        QMessageBox::No
    );


    confirmBox.setStyleSheet(
        QStringLiteral(R"(

            QMessageBox {
                background-color:#08111F;
            }

            QMessageBox QLabel {
                color:#EAF3FF;
                font-size:13px;
            }

            QPushButton {
                min-width:90px;
                min-height:34px;

                background-color:#10233B;
                color:#DCEBFF;

                border:1px solid #315A82;
                border-radius:8px;

                padding:6px 12px;

                font-weight:600;
            }

            QPushButton:hover {
                background-color:#173656;
                border-color:#60A5FA;
            }

        )")
    );


    if (confirmBox.exec()
        != QMessageBox::Yes) {

        return;
    }


    // =========================================================
    // Progress dialog
    // =========================================================

    auto *progress =
        new QProgressDialog(
            QStringLiteral(
                "正在通过 TCP Socket "
                "向电桩发送重启指令..."
            ),
            QString(),
            0,
            100,
            this
        );


    progress->setWindowTitle(
        QStringLiteral(
            "远程重启"
        )
    );

    progress->setCancelButton(
        nullptr
    );

    progress->setWindowModality(
        Qt::ApplicationModal
    );

    progress->setAutoClose(
        false
    );

    progress->setAutoReset(
        false
    );

    progress->setMinimumDuration(
        0
    );

    progress->setValue(
        0
    );


    progress->setStyleSheet(
        QStringLiteral(R"(

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

        )")
    );


    progress->show();


    // =========================================================
    // Animate progress while waiting for Socket response
    // =========================================================

    auto *progressTimer =
        new QTimer(progress);

    progressTimer->setInterval(
        100
    );


    connect(
        progressTimer,
        &QTimer::timeout,
        progress,
        [progress]()
        {
            const int current =
                progress->value();

            if (current < 95) {

                progress->setValue(
                    qMin(
                        95,
                        current + 5
                    )
                );
            }
        }
    );


    progressTimer->start();


    // =========================================================
    // Socket client
    // =========================================================

    auto *client =
        new ChargerCommandClient(this);


    connect(
        client,
        &ChargerCommandClient::restartFinished,
        this,
        [this,
         client,
         progress,
         progressTimer,
         id,
         style = confirmBox.styleSheet()](
            int,
            bool success,
            const QString &socketMessage)
        {
            progressTimer->stop();

            progress->setValue(
                100
            );

            progress->close();

            progress->deleteLater();


            // ==============================================
            // Socket failed
            // ==============================================

            if (!success) {

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
                    QStringLiteral(
                        "%1\n\n"
                        "请确认 charger_simulator "
                        "正在运行，并监听 "
                        "127.0.0.1:45454。"
                    ).arg(
                        socketMessage
                    )
                );

                errorBox.setStandardButtons(
                    QMessageBox::Ok
                );

                errorBox.setStyleSheet(
                    style
                );

                errorBox.exec();


                client->deleteLater();

                return;
            }


            // ==============================================
            // Charger acknowledged command.
            // Now update local database state.
            // ==============================================

            QString errorMessage;


            if (!m_chargerService
                     .restartCharger(
                         id,
                         errorMessage
                     )) {

                QMessageBox errorBox(this);

                errorBox.setIcon(
                    QMessageBox::Critical
                );

                errorBox.setWindowTitle(
                    QStringLiteral(
                        "状态更新失败"
                    )
                );

                errorBox.setText(
                    QStringLiteral(
                        "电桩已经通过 Socket "
                        "确认重启，"
                        "但本地数据库状态更新失败。\n\n%1"
                    ).arg(
                        errorMessage
                    )
                );

                errorBox.setStandardButtons(
                    QMessageBox::Ok
                );

                errorBox.setStyleSheet(
                    style
                );

                errorBox.exec();


                client->deleteLater();

                return;
            }


            // ==============================================
            // Refresh UI
            // ==============================================

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
                    "TCP Socket 指令执行成功。\n\n"
                    "电桩 %1 已完成重启，"
                    "并恢复为空闲状态。"
                ).arg(
                    id
                )
            );

            okBox.setStandardButtons(
                QMessageBox::Ok
            );

            okBox.setStyleSheet(
                style
            );

            okBox.exec();


            client->deleteLater();
        }
    );


    // =========================================================
    // Send command
    // =========================================================

    client->restartCharger(
        id
    );
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

