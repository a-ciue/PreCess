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
 * 光标位于分组内时给出内 4 + 中心落点；位于区域空白处时按边缘判定
 * 外 4 落点。指示器几何由 ClassicIndicators 生成。
 */
class DropIndicatorOverlay
{
public:
    //! @brief 光标在分组内的落点；不在分组内返回 DropLocation_None
    static DropLocation locationInGroup(const QRect& group_rect, const QPoint& global_pos);
    //! @brief 光标在区域空白处的落点（仅靠近边缘时命中外 4）
    static DropLocation locationInArea(const QRect& area_rect, const QPoint& global_pos);
};

}
