// ============================================================
// Haversine 距离单元测试（CTest: ncs_geo_test）
// 验证用例：
//   1) 北京天安门 → 上海人民广场 ≈ 1067 km（误差 < 20 km）
//   2) 同点距离 = 0
//   3) 良乡 → 中关村 ≈ 31 km（同城量级，UC-U-02 验收参考）
// ============================================================
#include <cmath>
#include <cstdio>

#include "core/util/geo_util.h"

namespace {

bool checkClose(const char *name, double actual, double expected, double tolerance)
{
    const bool ok = std::abs(actual - expected) <= tolerance;
    std::printf("[%s] %s: actual=%.2f expected=%.2f tol=%.2f\n",
                ok ? "PASS" : "FAIL", name, actual, expected, tolerance);
    return ok;
}

} // namespace

int main()
{
    bool ok = true;

    // 1) 天安门(39.9087, 116.3975) → 人民广场(31.2304, 121.4737)
    ok &= checkClose("Beijing->Shanghai",
                     core::GeoUtil::haversineKm(39.9087, 116.3975, 31.2304, 121.4737),
                     1067.0, 20.0);

    // 2) 同点距离为 0
    ok &= checkClose("same-point-zero",
                     core::GeoUtil::haversineKm(39.9, 116.4, 39.9, 116.4), 0.0, 1e-6);

    // 3) 良乡(39.7313,116.1432) → 中关村(39.9845,116.3158) 约 31 km
    ok &= checkClose("Liangxiang->Zhongguancun",
                     core::GeoUtil::haversineKm(39.7313, 116.1432, 39.9845, 116.3158),
                     31.0, 8.0);

    // 4) 良乡 → 国贸CBD(39.9087,116.4590) 约 33 km
    ok &= checkClose("Liangxiang->Guomao",
                     core::GeoUtil::haversineKm(39.7313, 116.1432, 39.9087, 116.4590),
                     33.0, 8.0);

    std::printf(ok ? "\nALL TESTS PASSED\n" : "\nSOME TESTS FAILED\n");
    return ok ? 0 : 1;
}
