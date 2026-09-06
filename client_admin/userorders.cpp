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

void MainWindow::showSelectedUserOrders()
{
    if (m_selectedUserId <= 0) {
        return;
    }

    auto it = std::find_if(
        m_userRecords.cbegin(), m_userRecords.cend(),
        [this](const AdminUserService::UserRecord &u) {
            return u.id == m_selectedUserId;
        });
    if (it == m_userRecords.cend()) {
        return;
    }

    QList<AdminUserService::OrderRecord> orders;
    QString errorMessage;
    if (!m_adminUserService.loadUserOrderHistory(
            m_selectedUserId, orders, errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("订单历史"), errorMessage);
        return;
    }

    const auto user = *it;

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("订单历史 · %1").arg(user.nickname));
    dialog.resize(940, 560);
    dialog.setModal(true);
    dialog.setStyleSheet(QStringLiteral(
        "QDialog { background:#0F1929; color:#EDF4F6; }"
        "QLabel { color:#AAB8C0; font-size:14px; font-weight:700; }"
        "QPushButton { background:#253244; color:#E6EDF0; border:1px solid #36475A; "
        "border-radius:8px; padding:9px 18px; font-size:14px; font-weight:800; }"
        "QPushButton:hover { background:#2D3C4F; }"));

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(22, 20, 22, 20);
    layout->setSpacing(12);

    auto *heading = new QLabel(
        QStringLiteral("订单历史 · %1 · %2").arg(user.nickname, maskPhone(user.phone)),
        &dialog);
    heading->setStyleSheet(QStringLiteral(
        "QLabel { color:#F5F7FA; font-size:22px; font-weight:900; }"));
    layout->addWidget(heading);

    auto *table = new QTableWidget(&dialog);
    table->setColumnCount(8);
    table->setHorizontalHeaderLabels({
        QStringLiteral("订单ID"),
        QStringLiteral("电桩"),
        QStringLiteral("电站"),
        QStringLiteral("开始时间"),
        QStringLiteral("结束时间"),
        QStringLiteral("电量(kWh)"),
        QStringLiteral("金额(元)"),
        QStringLiteral("状态")
    });
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(false);
    table->setShowGrid(false);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->setMinimumHeight(360);
    table->setStyleSheet(QStringLiteral(
        "QTableWidget { background:#101A2B; color:#EAF0F3; border:1px solid #27394D; "
        "border-radius:10px; font-size:13px; }"
        "QHeaderView::section { background:#1A2739; color:#8FA1AC; border:none; "
        "padding:11px 7px; font-weight:800; }"
        "QTableWidget::item { padding:9px 6px; border-bottom:1px solid #1D2B3C; }"));

    for (const auto &o : orders) {
        const int row = table->rowCount();
        table->insertRow(row);

        const QString statusText =
            o.status == 0 ? QStringLiteral("充电中")
            : (o.status == 2 ? QStringLiteral("已完成")
                              : QStringLiteral("其他"));

        const QStringList values = {
            QString::number(o.id),
            o.chargerNo,
            o.stationName,
            o.startTime.isEmpty() ? QStringLiteral("--") : o.startTime,
            o.endTime.isEmpty() ? QStringLiteral("--") : o.endTime,
            QString::number(o.energy, 'f', 2),
            QString::number(o.amount, 'f', 2),
            statusText
        };

        for (int col = 0; col < values.size(); ++col) {
            auto *cell = new QTableWidgetItem(values.at(col));
            cell->setTextAlignment(Qt::AlignCenter);
            if (col == 7) {
                cell->setForeground(QBrush(QColor(
                    o.status == 0 ? QStringLiteral("#F6A648")
                                  : (o.status == 2 ? QStringLiteral("#10B981")
                                                   : QStringLiteral("#AAB8C0")))));
            }
            table->setItem(row, col, cell);
        }
    }

    if (orders.isEmpty()) {
        table->setRowCount(1);
        auto *empty = new QTableWidgetItem(QStringLiteral("暂无订单历史"));
        empty->setTextAlignment(Qt::AlignCenter);
        empty->setForeground(QBrush(QColor(QStringLiteral("#7E909B"))));
        table->setItem(0, 0, empty);
        table->setSpan(0, 0, 1, 8);
    }

    layout->addWidget(table, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("关闭"));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected,
            &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted,
            &dialog, &QDialog::accept);

    dialog.exec();
}

