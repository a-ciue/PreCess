/**
 * @file ClassicIndicators.h
 * @brief 经典拖放指示器几何：内 4 + 中心 + 外 4，共 9 个
 */

#pragma once

#include "DockTypes.h"

#include <QList>
#include <QRect>

namespace dock {

/**
 * @brief 经典指示器几何计算
 *
 * 指示器不直接绘制（视图层负责），此处只给出 9 个落点的箭头矩形，
 * 供 QML 叠加层按悬停落点显示对应箭头。
 */
class ClassicIndicators
{
public:
    //! @brief 单个指示器：落点与箭头矩形（DropArea 局部坐标）
    struct Indicator {
        DropLocation location = DropLocation_None;
        QRect rect;
    };

    /**
     * @brief 计算全部 9 个指示器几何
     * @param area_rect 停靠区域矩形（DropArea 局部坐标）
     * @param hovered_group_rect 悬停分组矩形；无效时内指示器以区域中心为准
     */
    static QList<Indicator> indicatorRects(const QRect& area_rect, const QRect& hovered_group_rect);
};

}
