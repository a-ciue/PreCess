/**
 * @file Config.h
 * @brief 停靠组件的默认行为常量
 *
 * 当前实现固定采用经典停靠组件的默认行为（无任何开关）：
 * 标题栏始终可见、选项卡仅在多于一个时显示且不可重排/无关闭按钮、
 * Classic 指示器、实时分隔条拖动。若未来需要可配置性再扩展。
 */

#pragma once

namespace dock {

struct Config {
    //! @brief 标题栏高度（像素）
    static constexpr int kTitleBarHeight = 30;
    //! @brief 选项卡栏高度（像素）
    static constexpr int kTabBarHeight = 28;
    //! @brief 分组内容边距（像素）
    static constexpr int kGroupContentsMargin = 1;
    //! @brief 触发拖拽所需的最小移动距离（像素）
    static constexpr int kStartDragDistance = 4;
    //! @brief 经典指示器距离窗口边缘的间距（像素）
    static constexpr int kIndicatorMargin = 10;
    //! @brief 经典指示器箭头尺寸（像素）
    static constexpr int kIndicatorSize = 40;
    //! @brief 浮动窗口自绘边框宽度（像素）
    static constexpr int kFloatingWindowBorder = 1;
    //! @brief 浮动窗口边缘缩放的命中宽度（像素）
    static constexpr int kFloatingWindowResizeMargin = 6;
};

}
