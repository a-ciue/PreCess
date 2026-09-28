/**
 * @file ZoneGeometry.cpp
 * @brief 经典指示器几何的实现
 */

#include "ZoneGeometry.h"

#include "DockMetrics.h"

namespace dock {

QList<ZoneGeometry::ZoneRect> ZoneGeometry::innerZones(const QRect& group_rect)
{
    QList<ZoneRect> zones;
    if (!group_rect.isValid())
        return zones;

    const int size = DockMetrics::kZoneSize;
    const int margin = DockMetrics::kZoneMargin;
    // QRect 右下为闭区间：用 (size-1)/2 居中，避免偶数尺寸产生 1px 偏移
    const int half = (size - 1) / 2;

    // 内指示器：以分组中心为中心的 3x3 排列
    const QPoint center = group_rect.center();
    const QRect center_rect(center.x() - half, center.y() - half, size, size);
    zones.append({ DropZone::Merge, center_rect });
    zones.append({ DropZone::InnerLeft, center_rect.translated(-(size + margin), 0) });
    zones.append({ DropZone::InnerRight, center_rect.translated(size + margin, 0) });
    zones.append({ DropZone::InnerTop, center_rect.translated(0, -(size + margin)) });
    zones.append({ DropZone::InnerBottom, center_rect.translated(0, size + margin) });
    return zones;
}

QList<ZoneGeometry::ZoneRect> ZoneGeometry::outerZones(const QRect& area_rect)
{
    QList<ZoneRect> zones;
    if (!area_rect.isValid())
        return zones;

    const int size = DockMetrics::kZoneSize;
    const int margin = DockMetrics::kZoneMargin;
    const int half = (size - 1) / 2;

    // 外指示器：贴区域四条边的中点
    const QPoint area_center = area_rect.center();
    zones.append({ DropZone::OuterLeft,
        QRect(area_rect.left() + margin, area_center.y() - half, size, size) });
    zones.append({ DropZone::OuterRight,
        QRect(area_rect.right() - margin - (size - 1), area_center.y() - half, size, size) });
    zones.append({ DropZone::OuterTop,
        QRect(area_center.x() - half, area_rect.top() + margin, size, size) });
    zones.append({ DropZone::OuterBottom,
        QRect(area_center.x() - half, area_rect.bottom() - margin - (size - 1), size, size) });

    return zones;
}

QList<ZoneGeometry::ZoneRect> ZoneGeometry::allZones(const QRect& area_rect,
    const QRect& hovered_group_rect)
{
    if (!area_rect.isValid())
        return {};

    const QRect group_rect = hovered_group_rect.isValid() ? hovered_group_rect : area_rect;
    QList<ZoneRect> zones = innerZones(group_rect);
    zones.append(outerZones(area_rect));
    return zones;
}

}
