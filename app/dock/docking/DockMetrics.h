/**
 * @file DockMetrics.h
 * @brief 停靠组件的默认行为常量
 *
 * 当前实现固定采用经典停靠组件的默认行为（无任何开关）：
 * 标题栏始终可见、多标签分组为标签模式（标签可重排、每个标签可关闭）、
 * Classic 指示器、实时分隔条拖动。若未来需要可配置性再扩展。
 */

#pragma once

#include <QPoint>
#include <QSize>

namespace dock {

struct DockMetrics {
    //! @brief 标题栏高度（像素）
    static constexpr int kTitleBarHeight = 30;
    //! @brief 触发拖拽所需的最小移动距离（像素）
    static constexpr int kStartDragDistance = 4;
    //! @brief 经典指示器距离窗口边缘的间距（像素）
    static constexpr int kZoneMargin = 10;
    //! @brief 经典指示器箭头尺寸（像素）
    static constexpr int kZoneSize = 40;

    //! @brief 分组视图的层 z（区域同步时归一，分隔条高于此层）
    static constexpr qreal kGroupLayerZ = 1.0;
    //! @brief 分隔条视图的层 z（高于分组，保证始终可拖动）
    static constexpr qreal kSeparatorLayerZ = 2.0;
    //! @brief 宿主视图置顶的 z 上限（避免 bringToFront 无界自增）
    static constexpr qreal kHostFrontMaxZ = 1000.0;

    //! @brief 拖拽浮窗无法取得源尺寸时的兜底尺寸（像素）
    static inline const QSize kDefaultFloatingWindowSize { 300, 200 };

    /**
     * @brief 是否达到拖拽启动阈值
     *
     * 各拖拽路径（会话 Armed 推进、视图层预判）统一使用曼哈顿距离判定，
     * 避免同一手势在不同路径下触发条件不一致。
     */
    static bool exceedsDragThreshold(const QPoint& delta)
    {
        return delta.manhattanLength() >= kStartDragDistance;
    }
};

}
