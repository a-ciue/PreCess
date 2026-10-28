/**
 * @file ScalarRenderRange.h
 * @brief 一个组件属性独立保存的原始值域、颜色映射范围和单元显示范围。
 */
#pragma once

#include <array>
#include <cmath>

/** @brief 范围参数与渲染启用状态分离，取消渲染不会丢失用户设置。 */
struct ScalarRenderRange {
    std::array<double, 2> original;
    std::array<double, 2> mapping;
    std::array<double, 2> display;
    bool filtered { false };

    /** @brief 首次选择属性时，两种范围都采用完整数据值域。 */
    explicit ScalarRenderRange(std::array<double, 2> range)
        : original(range)
        , mapping(range)
        , display(range)
    {
    }

    /** @brief 接受有限的有序闭区间，不将手动数值截断到原始值域。 */
    bool setRange(bool display_mode, double lower, double upper)
    {
        if (!std::isfinite(lower) || !std::isfinite(upper) || lower > upper)
            return false;
        if (display_mode) {
            display = { lower, upper };
            filtered = true;
        } else {
            mapping = { lower, upper };
        }
        return true;
    }

    /** @brief 仅重置当前编辑的范围；显示重置须连缺失值一起恢复原始数据。 */
    void resetRange(bool display_mode)
    {
        if (display_mode) {
            display = original;
            filtered = false;
        } else {
            mapping = original;
        }
    }
};
