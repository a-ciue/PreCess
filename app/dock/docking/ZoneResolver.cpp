/**
 * @file ZoneResolver.cpp
 * @brief 落点计算的实现
 */

#include "ZoneResolver.h"

#include "ZoneGeometry.h"
#include "DockMetrics.h"

namespace dock {

DropZone ZoneResolver::zoneInGroup(const QRect& group_rect, const QPoint& global_pos)
{
    if (!group_rect.contains(global_pos))
        return DropZone::None;

    // 与绘制的方框一致：命中哪个方框即选择哪个落点；
    // 方框外扩半个间距补齐相邻间隙（方框之间无死区）
    const int tolerance = DockMetrics::kZoneMargin / 2;
    const QList<ZoneGeometry::ZoneRect> zones
        = ZoneGeometry::innerZones(group_rect);
    for (const ZoneGeometry::ZoneRect& indicator : zones) {
        if (indicator.rect.adjusted(-tolerance, -tolerance, tolerance, tolerance).contains(global_pos))
            return indicator.location;
    }
    return DropZone::None;
}

DropZone ZoneResolver::zoneInRegion(const QRect& area_rect, const QPoint& global_pos)
{
    if (!area_rect.contains(global_pos))
        return DropZone::None;

    // 与绘制的方框一致：命中哪个外方框即选择哪个落点；
    // 方框外扩半个间距补齐相邻间隙
    const int tolerance = DockMetrics::kZoneMargin / 2;
    const QList<ZoneGeometry::ZoneRect> zones
        = ZoneGeometry::outerZones(area_rect);
    for (const ZoneGeometry::ZoneRect& indicator : zones) {
        if (indicator.rect.adjusted(-tolerance, -tolerance, tolerance, tolerance).contains(global_pos))
            return indicator.location;
    }
    return DropZone::None;
}

}
