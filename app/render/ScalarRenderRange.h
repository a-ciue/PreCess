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
    /** @brief 绑定后两种范围联动；解绑保留当前数值。 */
    bool linked { false };

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
        const std::array<double, 2> range { lower, upper };
        if (display_mode || linked) {
            // 映射未变化时保留重置后的未筛选状态，重新应用不能暗中启用筛选。
            filtered = filtered || display_mode || display != range;
            display = range;
        }
        if (!display_mode || linked)
            mapping = range;
        return true;
    }

    /** @brief 重置当前范围，绑定时两份一起重置；显示重置也恢复缺失值单元。 */
    void resetRange(bool display_mode)
    {
        if (display_mode || linked) {
            display = original;
            filtered = false;
        }
        if (!display_mode || linked)
            mapping = original;
    }
};
