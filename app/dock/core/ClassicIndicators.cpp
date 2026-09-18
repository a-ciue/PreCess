/**
 * @file ClassicIndicators.cpp
 * @brief 经典指示器几何的实现
 */

#include "ClassicIndicators.h"

#include "Config.h"

namespace dock {

QList<ClassicIndicators::Indicator> ClassicIndicators::innerIndicatorRects(const QRect& group_rect)
{
    QList<Indicator> indicators;
    if (!group_rect.isValid())
        return indicators;

    const int size = Config::kIndicatorSize;
    const int margin = Config::kIndicatorMargin;
    // QRect 右下为闭区间：用 (size-1)/2 居中，避免偶数尺寸产生 1px 偏移
    const int half = (size - 1) / 2;

    // 内指示器：以分组中心为中心的 3x3 排列
    const QPoint center = group_rect.center();
    const QRect center_rect(center.x() - half, center.y() - half, size, size);
    indicators.append({ DropLocation_Center, center_rect });
    indicators.append({ DropLocation_Left, center_rect.translated(-(size + margin), 0) });
    indicators.append({ DropLocation_Right, center_rect.translated(size + margin, 0) });
    indicators.append({ DropLocation_Top, center_rect.translated(0, -(size + margin)) });
    indicators.append({ DropLocation_Bottom, center_rect.translated(0, size + margin) });
    return indicators;
}

QList<ClassicIndicators::Indicator> ClassicIndicators::outerIndicatorRects(const QRect& area_rect)
{
    QList<Indicator> indicators;
    if (!area_rect.isValid())
        return indicators;

    const int size = Config::kIndicatorSize;
    const int margin = Config::kIndicatorMargin;
    const int half = (size - 1) / 2;

    // 外指示器：贴区域四条边的中点
    const QPoint area_center = area_rect.center();
    indicators.append({ DropLocation_OutterLeft,
        QRect(area_rect.left() + margin, area_center.y() - half, size, size) });
    indicators.append({ DropLocation_OutterRight,
        QRect(area_rect.right() - margin - (size - 1), area_center.y() - half, size, size) });
    indicators.append({ DropLocation_OutterTop,
        QRect(area_center.x() - half, area_rect.top() + margin, size, size) });
    indicators.append({ DropLocation_OutterBottom,
        QRect(area_center.x() - half, area_rect.bottom() - margin - (size - 1), size, size) });

    return indicators;
}

QList<ClassicIndicators::Indicator> ClassicIndicators::indicatorRects(const QRect& area_rect,
    const QRect& hovered_group_rect)
{
    if (!area_rect.isValid())
        return {};

    const QRect group_rect = hovered_group_rect.isValid() ? hovered_group_rect : area_rect;
    QList<Indicator> indicators = innerIndicatorRects(group_rect);
    indicators.append(outerIndicatorRects(area_rect));
    return indicators;
}

}
