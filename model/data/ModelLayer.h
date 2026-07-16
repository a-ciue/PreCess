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
class TopoDS_Shape;

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

    /**
     * @brief 向已有模型增加一个几何组件，并初始化几何子形状索引。
     *
     * @return 新组件的全局 ID。
     */
    Index addGeometryComponent(Index model_id, std::unique_ptr<ComponentData> component);

    /**
     * @brief 将新形状追加到已有几何组件，并重新建立子形状索引。
     *
     * @return 被更新的组件 ID。
     */
    Index appendGeometryShape(Index component_id, TopoDS_Shape shape);

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
    std::optional<ModelOperator> getModelOperator(Index model_id) const;
    ModelData* modelById(Index model_id) const;
    std::optional<ComponentOperator> getComponentOperator(Index component_id);

    ComponentData* findComponent(Index component_id) const;

    GeometryRegistry& geomRegistry();
    const GeometryRegistry& geomRegistry() const;

    const std::vector<std::array<double, 3>>& globalPoints() const;

    MeshIDMap& edgeIdMap();
    const MeshIDMap& edgeIdMap() const;

    // 将运行期新生成的点追加到全局点池，返回这批点的第一个全局点 ID。
    Index appendGlobalPoints(const std::vector<std::array<double, 3>>& pts);

private:
    Index allocateComponentId() noexcept;

    GeometryRegistry geom_registry_;

    std::unordered_map<Index, std::unique_ptr<ModelData>> models_;
    std::unordered_map<Index, std::unique_ptr<ComponentData>> components_; // 全局组件池
    std::unordered_map<Index, Index> component_to_model_;
    Index max_index_{ -1 }; //!< 最大索引值，用于唯一标识模型
    Index next_component_id_ { 0 }; //!< component_id 全局发号器（只增不减）

    std::vector<std::array<double, 3>> global_points_;
    MeshIDMap edge_id_map_; // 先只维护 edge 的 global->local

    ModelObserver* observer_{ nullptr };                     //!< 全局模型观察者，用于捕获模型事件

    friend class QModelQuery;
};
#endif // MODEL_MANAGER_H
