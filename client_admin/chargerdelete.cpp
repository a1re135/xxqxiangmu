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

void MainWindow::deleteSelectedCharger()
{
    const int id = selectedChargerId();
    if (id <= 0) {
        QMessageBox::information(this, QStringLiteral("删除电桩"),
                                 QStringLiteral("请先选择一个电桩。"));
        return;
    }

    if (selectedChargerStatus() == 1) {
        QMessageBox::warning(this, QStringLiteral("删除电桩"),
                             QStringLiteral("该电桩正在使用中，禁止删除。"));
        return;
    }

    QString chargerNo;
    for (const auto &r : m_chargerRecords) {
        if (r.id == id) {
            chargerNo = r.chargerNo;
            break;
        }
    }

    const auto result = QMessageBox::warning(
        this,
        QStringLiteral("确认删除"),
        QStringLiteral("确定删除电桩「%1」吗？此操作不可撤销。").arg(chargerNo),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (result != QMessageBox::Yes) return;

    QString errorMessage;
    if (!m_chargerService.deleteCharger(id, errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("删除电桩"), errorMessage);
        return;
    }

    refreshChargerManagement();
}

