#pragma once
#include "Core.h"

class ModelLayer;
class ModelObserver;
struct ComponentData;
struct MeshData;
struct GeometryData;
class ModelData;
class TopoDS_Shape;

class ComponentOperator {
public:
    ComponentOperator(Index component_id,
        ComponentData& component,
        ModelLayer& mgr,
        ModelObserver* observer = nullptr,
        Index model_id = -1) noexcept;

    Index componentId() const noexcept { return component_id_; }

    ComponentData& component() const noexcept { return *component_; }
    MeshData* mesh() const noexcept;
    GeometryData* geometry() const noexcept;

    ModelLayer& manager() const noexcept { return *mgr_; }
    ModelObserver* observer() const noexcept { return observer_; }

    Index modelId() const noexcept;
    ModelData* model() const;

    /**
     * @brief 把一条几何边物化为边单元（写入 edge_vertices_），使其可挂属性、参与边渲染
     *
     * 幂等：该边已物化时直接返回既有 cell 序号。同步分配全局边 id（MeshIDMap），
     * 并调用 notifyChanged()（失效邻接索引、通知观察者）。
     *
     * @param p0 边端点 id（与 MeshData 连通性同一键空间，当前为全局点 id）
     * @param p1 边另一端点 id
     * @return 物化边在 edge_vertices_ 中的 cell 序号
     * @throw std::runtime_error 组件无网格
     * @throw std::invalid_argument 端点非法（负值或两端点相同）
     */
    Index materializeEdge(Index p0, Index p1);

    void notifyChanged() const;

    /**
     * @brief 将新形状写入当前组件；无几何时初始化，否则追加并重建子形状索引。
     * @return 当前组件 ID。
     */
    Index appendGeometryShape(TopoDS_Shape shape);

    void removeMesh();
    void removeGeometry();

private:
    Index component_id_ { -1 };
    Index model_id_ { -1 };
    ComponentData* component_ { nullptr };
    ModelLayer* mgr_ { nullptr };
    ModelObserver* observer_ { nullptr };
};