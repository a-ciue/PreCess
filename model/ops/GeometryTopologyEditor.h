#pragma once

#include <TopoDS_Shape.hxx>

/**
 * @brief 提供不依赖界面和模型管理的基础 OCC 拓扑编辑函数。
 */
class GeometryTopologyEditor {
public:
    /**
     * @brief 从根形状中删除一个顶层独立 Vertex、Edge、Face 或 Solid。
     *
     * @param root 当前几何根形状。
     * @param target 要删除的形状，必须是根形状本身或根 Compound 的直接子形状。
     * @param delete_children 是否同时删除目标的独占下级拓扑；为 false 时将目标的直接
     * 下级拓扑提升为根 Compound 的直接子形状。
     * @return 删除后的根形状；没有任何剩余拓扑时返回空 Shape。
     *
     * @throws std::invalid_argument 输入为空、类型不支持或目标不是顶层独立形状。
     * @throws std::runtime_error 编辑后的拓扑无效。
     */
    static TopoDS_Shape removeTopLevelShape(
        const TopoDS_Shape& root,
        const TopoDS_Shape& target,
        bool delete_children);
};
