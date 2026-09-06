#pragma once
// ============================================================
// 球面距离工具（Haversine 公式），UC-U-02「附近充电站查询」使用。
// 纯函数、无平台依赖（NFR-C-01）。
// ============================================================

namespace core {

class GeoUtil {
public:
    // 地球平均半径（公里）
    static constexpr double kEarthRadiusKm = 6371.0088;

    static double toRadians(double degrees);

    // 两点球面直线距离，单位：公里。
    // 参数均为 WGS-84 十进制度数（纬度 latitude，经度 longitude）。
    static double haversineKm(double lat1, double lng1, double lat2, double lng2);
};

} // namespace core
