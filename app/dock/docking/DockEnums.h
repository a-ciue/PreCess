/**
 * @file DockEnums.h
 * @brief 停靠组件的公共枚举与判定辅助
 */

#pragma once

#include <QObject>

#include <optional>

namespace dock {

Q_NAMESPACE

//! @brief 停靠方位
enum class DockEdge {
    Left,
    Top,
    Right,
    Bottom
};
Q_ENUM_NS(DockEdge)

//! @brief 面板首次放置时的可见性
enum class PanelLaunch {
    Visible,
    Hidden
};
Q_ENUM_NS(PanelLaunch)

//! @brief 拖放落点：分组内 5 个 + 区域边缘 4 个
enum class DropZone {
    None,
    InnerLeft,
    InnerTop,
    InnerRight,
    InnerBottom,
    Merge,
    OuterLeft,
    OuterTop,
    OuterRight,
    OuterBottom
};
Q_ENUM_NS(DropZone)

//! @brief 是否为分组内落点（方位或合并）
inline bool isInnerZone(DropZone zone)
{
    switch (zone) {
    case DropZone::InnerLeft:
    case DropZone::InnerTop:
    case DropZone::InnerRight:
    case DropZone::InnerBottom:
    case DropZone::Merge:
        return true;
    default:
        return false;
    }
}

//! @brief 是否为区域边缘落点
inline bool isOuterZone(DropZone zone)
{
    switch (zone) {
    case DropZone::OuterLeft:
    case DropZone::OuterTop:
    case DropZone::OuterRight:
    case DropZone::OuterBottom:
        return true;
    default:
        return false;
    }
}

//! @brief 落点对应的停靠方位；仅分组内/边缘的方位落点有值（合并与无落点为空）
inline std::optional<DockEdge> edgeForZone(DropZone zone)
{
    switch (zone) {
    case DropZone::InnerLeft:
    case DropZone::OuterLeft:
        return DockEdge::Left;
    case DropZone::InnerTop:
    case DropZone::OuterTop:
        return DockEdge::Top;
    case DropZone::InnerRight:
    case DropZone::OuterRight:
        return DockEdge::Right;
    case DropZone::InnerBottom:
    case DropZone::OuterBottom:
        return DockEdge::Bottom;
    default:
        return std::nullopt;
    }
}

}
