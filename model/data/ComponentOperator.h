#pragma once
#include "Core.h"

#include <array>
#include <memory>
#include <vector>

class ModelLayer;
struct ComponentData;
struct MeshData;
struct GeometryData;
class ModelData;
class TopoDS_Shape;

/**
 * @brief 组件数据修改类别：决定标脏时是否立即失效网格邻接懒表
 */
enum class MeshEditKind {
    Topology, //!< 拓扑/坐标变更：立即失效 mesh_adjacency 懒表（查询即时正确）
    NonTopology, //!< 仅属性等附着数据变更：不动拓扑，邻接懒表保持有效
};

/**
 * @brief 组件数据的统一写入口：写必脏
 *
 * 所有可写入口（editableMesh(kind) 与语义化方法）自动经 ModelLayer::markComponentDirty
 * 标脏——Topology 类立即失效邻接懒表 + 记入待通知集合（去重）；通知不即时发出，
 * 由操作边界 ModelLayer::flushNotifications() 统一发 notifyComponentChanged。
 * 只读访问（component()/mesh()）不标脏；插件不再持有通知职责。
 */
class ComponentOperator {
public:
    ComponentOperator(Index component_id,
        ComponentData& component,
        ModelLayer& mgr,
        Index model_id = -1) noexcept;

    Index componentId() const noexcept { return component_id_; }

    const ComponentData& component() const noexcept { return *component_; }
    const MeshData* mesh() const noexcept;
    GeometryData* geometry() const noexcept;

    ModelLayer& manager() const noexcept { return *mgr_; }

    Index modelId() const noexcept;
    ModelData* model() const;

    /**
     * @brief 申请可写网格数据（获取即标脏：Topology 立即失效邻接懒表 + 记入待通知集合）
     * @param kind 修改类别，默认 Topology；仅写属性等附着数据时传 NonTopology 避免过度失效
     * @throw std::runtime_error 组件无网格
     */
    MeshData& editableMesh(MeshEditKind kind = MeshEditKind::Topology);

    //! @brief 运行期加点（原子四连：push、vertex_count_ 同步、pointIdMap 分配 gid、point_global_ids_ 追加）并标脏
    Index appendPoint(std::array<double, 3> pos);

    /**
     * @brief 追加面单元（空 face_vertices_offset_ 先补 {0}）并标脏
     * @param local_point_ids 面顶点（组件内局部点 id，与 MeshData 连通性同一键空间）
     * @return 新面单元序号
     * @throw std::invalid_argument 点数为 0 或局部 id 越界
     */
    Index appendFace(const std::vector<Index>& local_point_ids);

    //! @brief 整网格替换（gid 纪律内建：释放旧点/边 gid → 就位 → ensure 点/边 gid → 标脏）
    void replaceMesh(std::unique_ptr<MeshData> mesh);

    /**
     * @brief 把一条几何边物化为边单元（写入 edge_vertices_），使其可挂属性、参与边渲染
     *
     * 幂等：该边已物化时直接返回既有 cell 序号。同步分配全局边 id（MeshIDMap），
     * 并标脏（失效邻接懒表；通知由操作边界 flush 统一发出）。
     *
     * @param p0 边端点 id（与 MeshData 连通性同一键空间，组件内局部点 id）
     * @param p1 边另一端点 id
     * @return 物化边在 edge_vertices_ 中的 cell 序号
     * @throw std::runtime_error 组件无网格
     * @throw std::invalid_argument 端点非法（负值或两端点相同）
     */
    Index materializeEdge(Index p0, Index p1);

    //! @brief 取组件当前状态的深拷贝快照（撤销重做/预览机制的统一原语）
    std::unique_ptr<ComponentData> takeSnapshot() const;

    /**
     * @brief 恢复快照：gid 对账（释放现有点/边 gid → 覆盖数据 → 按快照原值 reclaim）后标脏
     * @note 若组件带几何，几何子形状索引经重建领新 gid（索引是派生缓存，gid 身份载体在
     *       GeometryRegistry 且无法 reclaim 原值），几何 gid 跨 undo 不保持——由"undo 后
     *       选择集清空"约定覆盖；mesh 侧点/边 gid 身份由 reclaim 保证。
     *       通知不即时发出，由操作边界 flushNotifications() 统一发出。
     * @throw std::runtime_error gid 对账失败（reclaim 冲突）
     */
    void restoreSnapshot(const ComponentData& snapshot);

    /**
     * @brief 将新形状写入当前组件；无几何时初始化，否则追加并重建子形状索引。
     * @return 当前组件 ID。
     */
    Index appendGeometryShape(TopoDS_Shape shape);

    /**
     * @brief 替换当前组件的几何根形状，并重建子形状索引；空 Shape 表示移除 Geometry。
     * @return 当前组件 ID。
     */
    Index replaceGeometryRoot(TopoDS_Shape shape);

    void removeMesh();
    void removeGeometry();

private:
    Index component_id_ { -1 };
    Index model_id_ { -1 };
    ComponentData* component_ { nullptr };
    ModelLayer* mgr_ { nullptr };
};