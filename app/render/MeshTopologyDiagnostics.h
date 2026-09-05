/**
 * @file MeshTopologyDiagnostics.h
 * @brief 渲染层使用的网格特殊拓扑实体与二面角边诊断
 */
#pragma once

#include "Core.h"

/**
 * @brief 一条可直接用于诊断渲染的网格边
 */
struct TopologyDiagnosticEdge {
    std::array<Index, 2> endpoints; //> 组件内局部点 id
    double dihedral_angle_degrees { -1.0 }; //> 无有效二面角时为 -1
};

/**
 * @brief 保存一次网格拓扑诊断得到的特殊实体集合
 */
struct MeshTopologyDiagnosticResult {
    std::vector<TopologyDiagnosticEdge> boundary_edges;
    std::vector<Index> boundary_faces;
    std::vector<TopologyDiagnosticEdge> non_manifold_edges;
    std::vector<Index> non_manifold_vertices;
    std::vector<TopologyDiagnosticEdge> isolated_edges;
    std::vector<Index> isolated_vertices;
    std::vector<TopologyDiagnosticEdge> manifold_edges; //> 恰好邻接两个面的边，保存二面角供范围筛选
};

/**
 * @brief 基于渲染网格数据计算特殊拓扑实体，不修改模型数据
 */
class MeshTopologyDiagnostics {
public:
    /**
     * @brief 对面网格和显式边执行拓扑诊断
     * @param mesh 待读取的渲染网格数据
     * @return 可直接用于诊断 Actor 的分类结果
     */
    static MeshTopologyDiagnosticResult analyze(const MeshDataVtk& mesh);
};
