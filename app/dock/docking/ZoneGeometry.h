/**
 * @file ZoneGeometry.h
 * @brief 经典拖放指示器几何：内 4 + 中心 + 外 4，共 9 个
 */

#pragma once

#include "DockEnums.h"

#include <QList>
#include <QRect>

namespace dock {

/**
 * @brief 经典指示器几何计算
 *
 * 指示器不直接绘制（视图层负责），此处只给出 9 个落点的箭头矩形，
 * 供 QML 叠加层按悬停落点显示对应箭头。
 */
class ZoneGeometry
{
public:
    //! @brief 单个指示器：落点与箭头矩形（DockRegion 局部坐标）
    struct ZoneRect {
        DropZone location = DropZone::None;
        QRect rect;
    };

    /**
     * @brief 计算全部 9 个指示器几何
     * @param area_rect 停靠区域矩形（DockRegion 局部坐标）
     * @param hovered_group_rect 悬停分组矩形；无效时内指示器以区域中心为准
     */
    static QList<ZoneRect> allZones(const QRect& area_rect, const QRect& hovered_group_rect);

    /**
     * @brief 计算分组内的 5 个指示器几何（上/下/左/右/合并）
     *
     * 与落点命中判定共用同一布局，保证"画在哪里就能点哪里"。
     */
    static QList<ZoneRect> innerZones(const QRect& group_rect);

    /**
     * @brief 计算区域四边的 4 个外指示器几何（贴边中点）
     *
     * 同样供绘制与命中判定共用。
     */
    static QList<ZoneRect> outerZones(const QRect& area_rect);
};

}
