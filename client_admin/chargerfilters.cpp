#include "mainwindow.h"
#include "mainwindowhelpers.h"
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

void MainWindow::applyChargerFilters()
{
    if (!m_chargerModel) return;

    const int stationId = m_stationFilter ? m_stationFilter->currentData().toInt() : -1;
    const int status = m_statusFilter ? m_statusFilter->currentData().toInt() : -1;
    const QString keyword = m_chargerSearch
        ? m_chargerSearch->text().trimmed()
        : QString();

    m_chargerModel->removeRows(0, m_chargerModel->rowCount());

    const QList<ChargerService::ChargerRecord> &records = m_chargerRecords;
    for (const auto &r : records) {
        if (stationId >= 0 && r.stationId != stationId) continue;
        if (status >= 0 && r.status != status) continue;
        if (!keyword.isEmpty() && !r.chargerNo.contains(keyword, Qt::CaseInsensitive)) continue;

        QList<QStandardItem *> row;
        auto *idItem = new QStandardItem(r.chargerNo);
        idItem->setData(r.id, Qt::UserRole);

        auto *stationItem = new QStandardItem(r.stationName);
        auto *typeItem = new QStandardItem(r.type == 1 ? QStringLiteral("快充") : QStringLiteral("慢充"));
        auto *powerItem = new QStandardItem(QString::number(r.power, 'f', 1));
        auto *statusItem = new QStandardItem(
            r.status == 1 ? QStringLiteral("在用")
                          : (r.status == 2 ? QStringLiteral("故障") : QStringLiteral("闲置")));
        auto *countItem = new QStandardItem(QString::number(r.totalCount));
        auto *minutesItem = new QStandardItem(
            QString::number(static_cast<double>(r.totalMinutes) / 60.0, 'f', 2));

        for (QStandardItem *item : {idItem, stationItem, typeItem, powerItem, statusItem, countItem, minutesItem}) {
            item->setEditable(false);
        }

        if (r.status == 1) {
            statusItem->setForeground(QBrush(QColor(QStringLiteral("#F6A648"))));
        } else if (r.status == 2) {
            statusItem->setForeground(QBrush(QColor(QStringLiteral("#F05252"))));
        } else {
            statusItem->setForeground(QBrush(QColor(QStringLiteral("#10B981"))));
        }

        idItem->setData(r.id, Qt::UserRole);
        m_chargerModel->appendRow(row = {
            idItem, stationItem, typeItem, powerItem, statusItem, countItem, minutesItem
        });
    }

    if (m_chargerTable->selectionModel()) {
        m_chargerTable->clearSelection();
    }

    if (m_chargerModel->rowCount() == 0) {
        // Keep an explicit empty-data line in the table rather than failing silently.
        QList<QStandardItem *> emptyRow;
        auto *empty = new QStandardItem(QStringLiteral("暂无符合条件的电桩"));
        empty->setForeground(QBrush(QColor(QStringLiteral("#7E909B"))));
        emptyRow.append(empty);
        while (emptyRow.size() < m_chargerModel->columnCount()) {
            auto *blank = new QStandardItem();
            blank->setEnabled(false);
            emptyRow.append(blank);
        }
        m_chargerModel->appendRow(emptyRow);
    }

    const int statusForButtons = selectedChargerStatus();
    const bool hasSelection = selectedChargerId() > 0;
    m_restartChargerButton->setEnabled(hasSelection && m_restartProgress == nullptr);
    m_deleteChargerButton->setEnabled(hasSelection && statusForButtons != 1);
    m_faultChargerButton->setEnabled(hasSelection && statusForButtons != 2);
    m_recoverChargerButton->setEnabled(hasSelection && statusForButtons == 2);
}


int MainWindow::selectedChargerId() const
{
    if (!m_chargerTable || !m_chargerTable->selectionModel()) {
        return 0;
    }

    const QModelIndex index = m_chargerTable->currentIndex();
    if (!index.isValid() || !m_chargerModel) {
        return 0;
    }

    bool ok = false;
    const int id = index.sibling(index.row(), 0).data(Qt::UserRole).toInt(&ok);
    return ok ? id : 0;
}


int MainWindow::selectedChargerStatus() const
{
    const int id = selectedChargerId();
    if (id <= 0) return -1;

    for (const auto &r : m_chargerRecords) {
        if (r.id == id) {
            return r.status;
        }
    }
    return -1;
}

