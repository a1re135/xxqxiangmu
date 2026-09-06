#ifndef MAINWINDOWHELPERS_H
#define MAINWINDOWHELPERS_H

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QMargins>
#include <QObject>
#include <QString>
#include <QWidget>
#include <QtCharts/QChart>

namespace MainWindowHelpers {
template <typename T>
T *uiObject(QObject *root, const QString &name)
{
    return root ? root->findChild<T *>(name) : nullptr;
}
inline QFont chartAxisFont(int pointSize)
{
    QFont font;
    font.setPointSize(pointSize);
    return font;
}
inline void styleChart(QChart *chart)
{
    if (!chart) return;
    chart->setBackgroundBrush(QBrush(QColor(QStringLiteral("#121D30"))));
    chart->setPlotAreaBackgroundBrush(QBrush(QColor(QStringLiteral("#121D30"))));
    chart->setPlotAreaBackgroundVisible(true);
    chart->setMargins(QMargins(8, 8, 8, 8));
    chart->legend()->setVisible(false);
}
inline QString maskPhone(const QString &phone)
{
    const QString p = phone.trimmed();
    if (p.size() < 7) return QStringLiteral("***");
    if (p.size() <= 7) return QStringLiteral("***") + p.right(2);
    return p.left(3) + QStringLiteral("****") + p.right(4);
}
}
using MainWindowHelpers::uiObject;
using MainWindowHelpers::chartAxisFont;
using MainWindowHelpers::styleChart;
using MainWindowHelpers::maskPhone;
#endif
