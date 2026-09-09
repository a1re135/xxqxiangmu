#ifndef NAVIGATIONDIALOG_H
#define NAVIGATIONDIALOG_H

#include <QDialog>
#include <QString>

#include "service/stationservice.h"

class QComboBox;
class QLabel;
class QNetworkAccessManager;
class NavigationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NavigationDialog(
        const core::StationListItem &station,
        double startLatitude,
        double startLongitude,
        const QString &startName,
        QWidget *parent = nullptr
    );

private:
    void openBrowserNavigation();

    core::StationListItem m_station;

    double m_startLatitude = 0.0;
    double m_startLongitude = 0.0;
    QString m_startName;

    QComboBox *m_travelMode = nullptr;
    QLabel *m_messageLabel = nullptr;

    // 检查腾讯地图网站是否可以连接。
    void checkNetwork();

    QNetworkAccessManager *m_networkManager = nullptr;
    QLabel *m_networkLabel = nullptr;

    bool m_networkChecking = false;
};

#endif // NAVIGATIONDIALOG_H
