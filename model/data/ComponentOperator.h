#pragma once
#include "Core.h"

#include <memory>

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
     * @param p0 边端点 id（与 MeshData 连通性同一键空间，组件内局部点 id）
     * @param p1 边另一端点 id
     * @return 物化边在 edge_vertices_ 中的 cell 序号
     * @throw std::runtime_error 组件无网格
     * @throw std::invalid_argument 端点非法（负值或两端点相同）
     */
    Index materializeEdge(Index p0, Index p1);

    void notifyChanged() const;

    //! @brief 取组件当前状态的深拷贝快照（撤销重做/预览机制的统一原语）
    std::unique_ptr<ComponentData> takeSnapshot() const;

    /**
     * @brief 恢复快照：gid 对账（释放现有点/边 gid → 覆盖数据 → 按快照原值 reclaim）后 notifyChanged()
     * @note 若组件带几何，几何子形状索引经重建领新 gid（索引是派生缓存，gid 身份载体在
     *       GeometryRegistry 且无法 reclaim 原值），几何 gid 跨 undo 不保持——由"undo 后
     *       选择集清空"约定覆盖；mesh 侧点/边 gid 身份由 reclaim 保证。
     * @throw std::runtime_error gid 对账失败（reclaim 冲突）
     */
    void restoreSnapshot(const ComponentData& snapshot);

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