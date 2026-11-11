/**
 * @file SurfacePrecision.h
 * @brief 曲面显示精细度的统一配置与校验。
 */
#pragma once

#include <array>

namespace surface_precision {
struct Settings {
    double deviation_coefficient;
    double angle_degrees;
};

inline constexpr std::array<Settings, 4> kLevels { {
    { 0.003, 30.0 },
    { 0.001, 20.0 },
    { 0.0003, 10.0 },
    { 0.0001, 5.0 },
} };
inline constexpr int kDefault = 1;

constexpr bool isValid(int precision)
{
    return precision >= 0 && precision < static_cast<int>(kLevels.size());
}
}
