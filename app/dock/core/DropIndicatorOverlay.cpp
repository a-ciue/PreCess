/**
 * @file DropIndicatorOverlay.cpp
 * @brief 落点计算的实现
 */

#include "DropIndicatorOverlay.h"

#include "Config.h"

#include <algorithm>
#include <cstdlib>

namespace dock {

DropLocation DropIndicatorOverlay::locationInGroup(const QRect& group_rect, const QPoint& global_pos)
{
    if (!group_rect.contains(global_pos))
        return DropLocation_None;

    const QPoint center = group_rect.center();
    const int dx = global_pos.x() - center.x();
    const int dy = global_pos.y() - center.y();
    const int half_width = std::max(1, group_rect.width() / 2);
    const int half_height = std::max(1, group_rect.height() / 2);

    // 中心区域：分组宽高的 1/4
    if (std::abs(dx) <= half_width / 2 && std::abs(dy) <= half_height / 2)
        return DropLocation_Center;

    // 归一化比较，避免宽扁分组总是命中上下方向
    const long long horizontal_weight = static_cast<long long>(std::abs(dx)) * group_rect.height();
    const long long vertical_weight = static_cast<long long>(std::abs(dy)) * group_rect.width();
    if (horizontal_weight >= vertical_weight)
        return dx < 0 ? DropLocation_Left : DropLocation_Right;
    return dy < 0 ? DropLocation_Top : DropLocation_Bottom;
}

DropLocation DropIndicatorOverlay::locationInArea(const QRect& area_rect, const QPoint& global_pos)
{
    if (!area_rect.contains(global_pos))
        return DropLocation_None;

    const int left = global_pos.x() - area_rect.left();
    const int right = area_rect.right() - global_pos.x();
    const int top = global_pos.y() - area_rect.top();
    const int bottom = area_rect.bottom() - global_pos.y();
    const int nearest = std::min(std::min(left, right), std::min(top, bottom));
    if (nearest > Config::kOuterDropMargin)
        return DropLocation_None;

    if (nearest == left)
        return DropLocation_OutterLeft;
    if (nearest == right)
        return DropLocation_OutterRight;
    if (nearest == top)
        return DropLocation_OutterTop;
    return DropLocation_OutterBottom;
}

}
