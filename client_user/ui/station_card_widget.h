#pragma once
// ============================================================
// 电站卡片控件（UC-U-02）
// 展示：站名、单价（元/度）、空闲数/总桩数、距离（公里，1 位小数）。
// ============================================================
#include <QFrame>

#include "../../core/service/stationservice.h"

namespace client_user {

class StationCardWidget : public QFrame {
    Q_OBJECT
public:
    explicit StationCardWidget(const core::StationListItem &item,
                               QWidget *parent = nullptr);

    core::StationListItem item() const { return m_item; }

signals:
    void clicked(int stationId);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void setupUi();

    core::StationListItem m_item;
};

} // namespace client_user
