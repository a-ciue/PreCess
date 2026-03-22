// Component.h
#pragma once

#include "Core.h" 
#include <memory>
#include <string>

struct MeshData;
struct SplineData;

// 日后在这里补充 CAD↔网格的映射结构
struct SplineMeshMap {
    // 先占位，后续再设计具体字段
    // std::unordered_map<Index, std::vector<Index>> cadFace_to_meshFaces;
};

/**
 * @brief 模型的组成单元：一个 Component 表示模型中的一个物体部件
 *
 * - 0/1 个 CAD 模型（SplineData）
 * - 0/1 个 网格模型（MeshData）
 * - 将来可挂属性（材料）、CAD↔网格映射、显示状态等
 */
struct Component {
    Index id { -1 };
    std::string name;

    // 数据部分：一个组件可以有 CAD，也可以有网格，也可以都有/都没有（初始化阶段）
    std::unique_ptr<MeshData> mesh; ///< 网格数据（来自网格导入或由 CAD 网格化生成）
    std::unique_ptr<SplineData> cad; ///< CAD 数据（来自 STEP 等）
    std::unique_ptr<SplineMeshMap> mapping; ///< CAD↔网格对应关系（可选，后续实现）

    // 组件级属性（后续会扩展 Property/Material）
    Index material_id { -1 }; ///< 材料/属性 ID（先留个 int 占位）
    Index source_xde_leaf_id { -1 };

    // 便捷判断
    bool hasMesh() const noexcept { return static_cast<bool>(mesh); }
    bool hasCad() const noexcept { return static_cast<bool>(cad); }
};