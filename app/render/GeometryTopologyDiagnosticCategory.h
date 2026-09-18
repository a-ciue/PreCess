/**
 * @file GeometryTopologyDiagnosticCategory.h
 * @brief 定义几何拓扑诊断的可视化类别。
 */
#pragma once

#include <cstddef>

/**
 * @brief 可独立显示的几何拓扑诊断类别。
 */
enum class GeometryTopologyDiagnosticCategory {
    BoundaryEdge,
    IsolatedEdge,
    NonManifoldEdge,
    SmallEdge,
    SmallFace,
    DuplicateFace,
    //! 几何自交：单个零件内部的面自身穿插（对应 Surface Repair > Self Intersections）。
    SelfIntersectingFace,
    //! 面干涉：不限零件的面-面互相穿插（对应 Geometry Interference Check）。
    InterferingFace,
    InvalidTopology,
    Count
};

//! @brief 几何拓扑诊断类别数量，由枚举末值统一推导。
inline constexpr std::size_t kGeometryTopologyDiagnosticCategoryCount
    = static_cast<std::size_t>(GeometryTopologyDiagnosticCategory::Count);
