#include "geo_util.h"

#include <cmath>

namespace core {

namespace {
// 避免依赖平台宏 M_PI（NFR-C-01：跨平台可移植）
constexpr double kPi = 3.14159265358979323846;
} // namespace

double GeoUtil::toRadians(double degrees)
{
    return degrees * kPi / 180.0;
}

double GeoUtil::haversineKm(double lat1, double lng1, double lat2, double lng2)
{
    const double dLat = toRadians(lat2 - lat1);
    const double dLng = toRadians(lng2 - lng1);
    const double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0)
                     + std::cos(toRadians(lat1)) * std::cos(toRadians(lat2))
                           * std::sin(dLng / 2.0) * std::sin(dLng / 2.0);
    // a 在数值上恒在 [0,1]，clamp 防止浮点误差导致 sqrt 负数
    const double clamped = std::min(1.0, std::max(0.0, a));
    const double c = 2.0 * std::atan2(std::sqrt(clamped), std::sqrt(1.0 - clamped));
    return kEarthRadiusKm * c;
}

} // namespace core
