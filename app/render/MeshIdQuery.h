/**
 * @file MeshIdQuery.h
 * @brief 渲染层向模型层查询网格单元 id 的窄接口
 *
 * vtkPart 保持不依赖 Qt/QML，模型层的 id 查询能力经本接口由桥接层（QRenderWindow）注入实现。
 * 后续其他邻接类查询（面、体等）也在本接口上扩展。
 */
#ifndef MESH_ID_QUERY_H
#define MESH_ID_QUERY_H
#include "Core.h"

#include <optional>

class IMeshIdQuery {
public:
    virtual ~IMeshIdQuery() = default;

    /**
     * @brief 按两端点反查边的稳定局部 id
     * @param component_id 组件 ID
     * @param p0 边端点 id（与 MeshData 连通性同一键空间，当前为全局点 id）
     * @param p1 边另一端点 id（与 p0 无序）
     * @return 命中返回稳定局部边 id（跨拓扑编辑有效）；未命中返回 std::nullopt
     */
    virtual std::optional<Index> findEdgeByEndpoints(Index component_id, Index p0, Index p1) const = 0;
};
#endif
