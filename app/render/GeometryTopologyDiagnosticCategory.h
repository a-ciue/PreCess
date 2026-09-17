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
    IntersectingFace,
    InvalidTopology,
    Count
};

//! @brief 几何拓扑诊断类别数量，由枚举末值统一推导。
inline constexpr std::size_t kGeometryTopologyDiagnosticCategoryCount
    = static_cast<std::size_t>(GeometryTopologyDiagnosticCategory::Count);
