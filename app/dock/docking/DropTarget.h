/**
 * @file DropTarget.h
 * @brief 拖放落点目标：区域 + 分组 + 落点 + 标签插入位置
 */

#pragma once

#include "DockEnums.h"

namespace dock {

class DockRegion;
class PanelGroup;

/**
 * @brief 拖放落点目标
 *
 * 由 DropResolver 从全局坐标解析出的单一结果，供悬停状态比较、
 * 指示器绘制与落点执行共用；tab_index 仅在 zone == Merge 且命中
 * 分组的标签栏/标题栏条带时有效（-1 表示不指定位置，按追加处理）。
 */
struct DropTarget {
    //! @brief 命中的停靠区域（空表示无命中）
    DockRegion* region = nullptr;
    //! @brief 命中的分组（外落点或无分组命中时为空）
    PanelGroup* group = nullptr;
    //! @brief 落点
    DropZone zone = DropZone::None;
    //! @brief 标签插入位置（0..count；-1 表示无效）
    int tab_index = -1;

    bool operator==(const DropTarget& other) const
    {
        return region == other.region && group == other.group
            && zone == other.zone && tab_index == other.tab_index;
    }

    bool operator!=(const DropTarget& other) const { return !(*this == other); }
};

}
