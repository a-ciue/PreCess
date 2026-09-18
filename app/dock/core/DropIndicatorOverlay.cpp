/**
 * @file DropIndicatorOverlay.cpp
 * @brief 落点计算的实现
 */

#include "DropIndicatorOverlay.h"

#include "ClassicIndicators.h"
#include "Config.h"

namespace dock {

DropLocation DropIndicatorOverlay::locationInGroup(const QRect& group_rect, const QPoint& global_pos)
{
    if (!group_rect.contains(global_pos))
        return DropLocation_None;

    // 与绘制的方框一致：命中哪个方框即选择哪个落点；
    // 方框外扩半个间距补齐相邻间隙（方框之间无死区）
    const int tolerance = Config::kIndicatorMargin / 2;
    const QList<ClassicIndicators::Indicator> indicators
        = ClassicIndicators::innerIndicatorRects(group_rect);
    for (const ClassicIndicators::Indicator& indicator : indicators) {
        if (indicator.rect.adjusted(-tolerance, -tolerance, tolerance, tolerance).contains(global_pos))
            return indicator.location;
    }
    return DropLocation_None;
}

DropLocation DropIndicatorOverlay::locationInArea(const QRect& area_rect, const QPoint& global_pos)
{
    if (!area_rect.contains(global_pos))
        return DropLocation_None;

    // 与绘制的方框一致：命中哪个外方框即选择哪个落点；
    // 方框外扩半个间距补齐相邻间隙
    const int tolerance = Config::kIndicatorMargin / 2;
    const QList<ClassicIndicators::Indicator> indicators
        = ClassicIndicators::outerIndicatorRects(area_rect);
    for (const ClassicIndicators::Indicator& indicator : indicators) {
        if (indicator.rect.adjusted(-tolerance, -tolerance, tolerance, tolerance).contains(global_pos))
            return indicator.location;
    }
    return DropLocation_None;
}

}
