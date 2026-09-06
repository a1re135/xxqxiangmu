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
        QMessageBox::information(this, QStringLiteral("远程重启"),
                                 QStringLiteral("请先选择一个电桩。"));
        return;
    }

    const int status = selectedChargerStatus();
    if (status == 1) {
        const auto result = QMessageBox::warning(
            this,
            QStringLiteral("确认远程重启"),
            QStringLiteral("该电桩正在充电，重启会中断用户充电。\n确定要继续吗？"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (result != QMessageBox::Yes) return;
    } else if (status == 2) {
        const auto result = QMessageBox::question(
            this,
            QStringLiteral("确认远程重启"),
            QStringLiteral("该电桩当前为故障状态。\n模拟下发重启指令并在 2 秒后恢复为空闲吗？"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);
        if (result != QMessageBox::Yes) return;
    } else {
        const auto result = QMessageBox::question(
            this,
            QStringLiteral("确认远程重启"),
            QStringLiteral("模拟下发远程重启指令，完成后该电桩将回到闲置状态。继续吗？"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (result != QMessageBox::Yes) return;
    }

    m_restartChargerId = id;
    m_restartProgress = new QProgressDialog(
        QStringLiteral("正在下发远程重启指令…"),
        QStringLiteral("取消"),
        0, 100,
        this);
    m_restartProgress->setWindowTitle(QStringLiteral("远程重启"));
    m_restartProgress->setWindowModality(Qt::WindowModal);
    m_restartProgress->setAutoClose(false);
    m_restartProgress->setAutoReset(false);
    m_restartProgress->setValue(0);
    m_restartProgress->setStyleSheet(QStringLiteral(
        "QProgressDialog { background:#121D30; color:#EAF0F3; }"
        "QLabel { color:#EAF0F3; font-size:14px; font-weight:800; }"
        "QProgressBar { background:#1B2736; border:1px solid #334255; border-radius:8px; "
        "height:16px; text-align:center; color:#EAF0F3; }"
        "QProgressBar::chunk { background:#10B981; border-radius:7px; }"));
    m_restartProgress->show();

    auto *timer = new QTimer(m_restartProgress);
    timer->setInterval(100);
    connect(timer, &QTimer::timeout, this, [this, timer]() {
        if (!m_restartProgress) return;

        const int next = qMin(100, m_restartProgress->value() + 5);
        m_restartProgress->setValue(next);

        if (next >= 100) {
            timer->stop();

            QString errorMessage;
            const bool ok = m_chargerService.restartCharger(m_restartChargerId, errorMessage);
            const QString detail = ok
                ? QStringLiteral("远程重启完成，电桩状态已恢复为闲置。")
                : QStringLiteral("远程重启失败：%1").arg(errorMessage);

            m_restartProgress->close();
            m_restartProgress->deleteLater();
            m_restartProgress = nullptr;
            const int restartedId = m_restartChargerId;
            m_restartChargerId = 0;

            if (!ok) {
                QMessageBox::warning(this, QStringLiteral("远程重启"), detail);
            } else {
                refreshChargerManagement();
                QMessageBox::information(this, QStringLiteral("远程重启"), detail);
            }

            Q_UNUSED(restartedId);
        }
    });
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

