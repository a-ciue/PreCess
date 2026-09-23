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
