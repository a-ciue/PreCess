/**
 * @file TopologyDiagnosticCategory.h
 * @brief 定义拓扑诊断可视化类别
 */
#pragma once

#include <cstddef>

/**
 * @brief 可独立显示的网格拓扑诊断类别
 */
enum class TopologyDiagnosticCategory {
    BoundaryEdge,
    BoundaryFace,
    NonManifoldEdge,
    NonManifoldVertex,
    IsolatedEdge,
    IsolatedVertex,
    DihedralEdge,
    Count
};

//! @brief 拓扑诊断类别数量，由枚举末值统一推导
inline constexpr std::size_t kTopologyDiagnosticCategoryCount
    = static_cast<std::size_t>(TopologyDiagnosticCategory::Count);
