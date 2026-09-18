/**
 * @file DropIndicatorOverlay.h
 * @brief 拖放落点计算（Classic 指示器的 9 个位置）
 */

#pragma once

#include "DockTypes.h"

#include <QPoint>
#include <QRect>

namespace dock {

/**
 * @brief 落点计算
 *
 * 与绘制的指示器方框共用同一布局：光标命中哪个方框即选择哪个落点
 * （分组内 5 个、区域边缘 4 个）；未命中任何方框无落点。
 */
class DropIndicatorOverlay
{
public:
    //! @brief 光标命中的分组内指示器方框；未命中返回 DropLocation_None
    static DropLocation locationInGroup(const QRect& group_rect, const QPoint& global_pos);
    //! @brief 光标在区域空白处的落点（仅靠近边缘时命中外 4）
    static DropLocation locationInArea(const QRect& area_rect, const QPoint& global_pos);
};

}
