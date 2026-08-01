/**
 * @file ModelLayer.h
 * @brief 负责管理多个模型实例的类
 *
 * ModelLayer 仅负责管理模型（添加、删除、查询、重命名）以及模型事件的发送。
 *
 * @author 徐昊阳 haoyangxu06@gmail.com
 * @date 2025/3/20
 */
#ifndef MODEL_MANAGER_H
#define MODEL_MANAGER_H
#include "ModelData.h"

#include "ModelOperator.h"
#include "GeometryRegistry.h"
#include "MeshIDMap.h"

#include <array>
#include <vector>
#include <unordered_map>
#include <optional>

class ModelObserver;  // 前向声明模型观察者类
class QModelQuery;      // 前向声明 QModelQuery 类
class ComponentOperator;
struct ModelSnapshot;   // 前向声明模型级结构快照

/**
 * @brief 负责管理多个 ModelData 实例的类
 *
 * ModelLayer 允许动态添加、删除和查找模型，通过Observer模式发送模型事件。
 * 使得 QML 层能够访问和控制网格数据。
 */
class ModelLayer {

public:
    /**
     * @brief 构造 ModelLayer 对象
     *
     * @param parent 父对象，默认为 nullptr
    * @param observer 模型观察者对象，用于捕获模型事件（默认 nullptr）
     */
    explicit ModelLayer(ModelObserver* observer = nullptr) :  observer_(observer) {}

    /**
     * @brief 添加一个模型
     *
     * @param model_name 新模型的名称
     * @param model 需要添加的模型对象
     */
    Index addModel(const std::string& model_name, ComponentDatas components);

    //! @brief 取整模型深拷贝快照（撤销 removeModel / 重做 addModel 用）
    std::unique_ptr<ModelSnapshot> takeModelSnapshot(Index model_id) const;

    /**
     * @brief 按快照原 id 恢复模型（组件入池、component_to_model_、gid reclaim、几何索引重建）
     * @return 恢复出的模型 id（即快照原 id）
     * @throw std::runtime_error 原 model_id/component_id 已被占用，或 gid reclaim 冲突
     */
    Index restoreModel(const ModelSnapshot& snapshot);

    /**
     * @brief 按组件自带 id 把组件插回指定模型（撤销 removeComponent / 重做 addGeometryComponent 用）
     * @throw std::runtime_error model 不存在或 component id 已被占用，或 gid reclaim 冲突
     */
    void restoreComponent(Index model_id, std::unique_ptr<ComponentData> component);

    /**
     * @brief 移除指定名称的模型
     *
     * @param model_id 需要移除的模型 ID
     */
    void removeModel(Index model_id);
    void removeComponent(Index component_id);

    /**
     * @brief 获取指定模型的操作接口对象
     *
     * 如果对应模型的 ModelOperator 不存在，则创建并返回新的 ModelOperator。
     * ModelOperator 封装模型数据的操作接口，用于执行命令等操作。
     *
     * @param model_id 模型 ID
     * @return 对应模型名称的 ModelOperator 对象指针
     */
    std::optional<ModelOperator> getModelOperator(Index model_id);
    ModelData* modelById(Index model_id) const;
    std::optional<ComponentOperator> getComponentOperator(Index component_id);

    ComponentData* findComponent(Index component_id) const;

    /**
     * @brief 根据全局几何面 ID 查找所属 Component
     * @param face_id 全局几何面 ID
     * @return 所属 Component ID；未找到时返回空
     */
    std::optional<Index> findComponentIdByGeometryFaceId(GeomFaceId face_id) const;

    GeometryRegistry& geomRegistry();
    const GeometryRegistry& geomRegistry() const;

    MeshIDMap& pointIdMap();
    const MeshIDMap& pointIdMap() const;

    MeshIDMap& edgeIdMap();
    const MeshIDMap& edgeIdMap() const;

private:
    Index allocateComponentId() noexcept;

    /**
     * @brief 按给定 id 把组件纳入指定模型（入全局池、登记 component_to_model_、gid 对账）
     *
     * gid 对账顺序：几何索引 ensureIndexBuilt（快照克隆体索引未建，此处重建领新 gid）→
     * 点 gid 先 reclaimPointGlobalIds 按原值回收再 ensurePointGlobalIds 补缺 →
     * 边 gid 先 reclaimEdgeGlobalIds 再 ensureEdgeGlobalIds 补缺；reclaim 与 ensure 幂等兼容，
     * addModel（gid 尚未分配，reclaim 自然无操作）与快照恢复共用本路径。
     * @throw std::runtime_error model 不存在或 component id 已被占用，或 gid reclaim 冲突
     */
    void adoptComponent(Index component_id, std::unique_ptr<ComponentData> component, Index model_id);

    GeometryRegistry geom_registry_;

    std::unordered_map<Index, std::unique_ptr<ModelData>> models_;
    std::unordered_map<Index, std::unique_ptr<ComponentData>> components_; // 全局组件池
    std::unordered_map<Index, Index> component_to_model_;
    Index max_index_{ -1 }; //!< 最大索引值，用于唯一标识模型
    Index next_component_id_ { 0 }; //!< component_id 全局发号器（只增不减）

    MeshIDMap point_id_map_; // 点的 global<->local（gid 为纯身份标识，坐标由组件 MeshData 自持）
    MeshIDMap edge_id_map_; // 边的 global->local

    ModelObserver* observer_{ nullptr };                     //!< 全局模型观察者，用于捕获模型事件

    friend class QModelQuery;
    friend class ModelOperator;
};
#endif // MODEL_MANAGER_H
