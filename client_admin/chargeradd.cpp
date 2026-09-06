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

#include <QFormLayout>
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
void MainWindow::addNewCharger()
{
    QList<ChargerService::StationOption> stations;
    QString errorMessage;
    if (!m_chargerService.loadStations(stations, errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("新增电桩"), errorMessage);
        return;
    }

    if (stations.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("新增电桩"),
                             QStringLiteral("当前没有可用电站，无法新增电桩。"));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("新增电桩"));
    dialog.setModal(true);
    dialog.resize(480, 330);
    dialog.setStyleSheet(QStringLiteral(
        "QDialog { background:#0F1929; color:#EDF4F6; }"
        "QLabel { color:#AAB8C0; font-size:14px; font-weight:700; }"
        "QLineEdit, QComboBox, QDoubleSpinBox { background:#182437; color:#EEF4F6; "
        "border:1px solid #314257; border-radius:8px; padding:8px 10px; min-height:20px; "
        "font-size:14px; }"
        "QLineEdit:focus, QComboBox:focus, QDoubleSpinBox:focus { border:1px solid #10B981; }"
        "QPushButton { background:#253244; color:#E6EDF0; border:1px solid #36475A; "
        "border-radius:8px; padding:8px 18px; font-size:14px; font-weight:800; }"
        "QPushButton:hover { background:#2D3C4F; }"
        "QDialogButtonBox QPushButton[accept=\"true\"] { background:#10B981; color:#071A15; border:none; }"));

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 22, 24, 22);
    layout->setSpacing(14);

    auto *heading = new QLabel(QStringLiteral("新增充电桩"), &dialog);
    heading->setStyleSheet(QStringLiteral("QLabel { color:#F5F7FA; font-size:22px; font-weight:900; }"));
    layout->addWidget(heading);

    auto *form = new QFormLayout();
    form->setSpacing(12);
    form->setLabelAlignment(Qt::AlignLeft);

    auto *stationCombo = new QComboBox(&dialog);
    for (const auto &station : stations) {
        stationCombo->addItem(station.name, station.id);
    }

    auto *numberEdit = new QLineEdit(&dialog);
    numberEdit->setPlaceholderText(QStringLiteral("例如 ST01-09"));
    numberEdit->setMaxLength(64);

    auto *typeCombo = new QComboBox(&dialog);
    typeCombo->addItem(QStringLiteral("慢充"), 0);
    typeCombo->addItem(QStringLiteral("快充"), 1);

    auto *powerSpin = new QDoubleSpinBox(&dialog);
    powerSpin->setRange(0.1, 1000.0);
    powerSpin->setDecimals(1);
    powerSpin->setSingleStep(0.5);
    powerSpin->setValue(7.0);
    powerSpin->setSuffix(QStringLiteral(" kW"));

    form->addRow(QStringLiteral("所属电站"), stationCombo);
    form->addRow(QStringLiteral("电桩编号"), numberEdit);
    form->addRow(QStringLiteral("类型"), typeCombo);
    form->addRow(QStringLiteral("功率"), powerSpin);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确认新增"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (numberEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dialog, QStringLiteral("新增电桩"),
                                 QStringLiteral("请输入电桩编号。"));
            numberEdit->setFocus();
            return;
        }

        QString addError;
        if (!m_chargerService.addCharger(
                stationCombo->currentData().toInt(),
                numberEdit->text().trimmed(),
                typeCombo->currentData().toInt(),
                powerSpin->value(),
                addError)) {
            QMessageBox::warning(&dialog, QStringLiteral("新增电桩"), addError);
            return;
        }

        dialog.accept();
    });

    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        refreshChargerManagement();
    }
}

