/**
 * @file MeshTopologyDiagnostics.h
 * @brief 网格特殊拓扑实体与二面角边诊断
 */
#pragma once

#include "Core.h"

struct MeshData;
class MeshAdjacency;

/**
 * @brief 计算网格特殊拓扑实体，结果仅为派生数据，不修改原网格
 */
class MeshTopologyDiagnostics {
public:
    /**
     * @brief 对面网格和显式边执行拓扑诊断
     * @param mesh 待读取的网格数据
     * @return 可用于渲染的分类结果
     */
    static MeshTopologyDiagnosticResult analyze(MeshAdjacency& adjacency, const MeshData& mesh);
};
