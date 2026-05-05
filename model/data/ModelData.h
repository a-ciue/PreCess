/**
 * @file ModelData.h
 *
 * @brief 负责管理和操作网格模型数据的核心类
 *
 * ModelData 类用于存储和处理网格模型数据，包括面（Patch）、块（Block）和组（Group）的管理。
 * 它提供了一系列函数用于网格操作，如网格划分、合并和重划分等，同时维护与 MeshActor 之间的关联，
 * 以便进行可视化和渲染。
 *
 * @author 徐昊阳 haoyangxu06\@gmail.com
 * @date 2025/3/8
 */
#ifndef MODEL_H
#define MODEL_H
#include "ComponentData.h"
#include "Selection.h"

#include <memory>
#include <optional>
#include <string>
#include <variant>

struct MeshData;
struct GeometryData;

//! @brief Model主要负责处理模型数据，先更新模型数据，再更新ModelActor调函数
/**
 *@brief 负责管理和操作网格模型数据的核心类
 *
 * ModelData 负责管理网格数据，包括 Patch、Block 和 Group 的存储、更新和操作。
 * 该类提供了网格划分、合并、重划分等功能，并维护与 MeshActor 之间的关联，
 * 以便进行可视化和渲染。
 */
class ModelData {
public:
    ModelData();
    ~ModelData();

    std::string model_name_;

    enum class Type {
        None,
        Mesh,
        Spline,
        Mixed
    };

    // 创建一个新组件
    ComponentData* createComponent(Index id, const std::string& name);
    const std::vector<Index>& componentIds() const noexcept;
    std::vector<Index>& componentIdsMut() noexcept;

    // 访问当前模型中的所有组件
    std::vector<std::unique_ptr<ComponentData>>& stagingcomponents();
    const std::vector<std::unique_ptr<ComponentData>>& stagingcomponents() const;

    /* ============ 构造（仅声明） ============ */
    explicit ModelData(std::unique_ptr<MeshData> mesh);
    explicit ModelData(std::unique_ptr<GeometryData> spline);

    ModelData(const ModelData& other) = delete;
    ModelData& operator=(const ModelData& other) = delete;
    ModelData(ModelData&& other) noexcept;
    ModelData& operator=(ModelData&& other) noexcept;

private:

    std::vector<std::unique_ptr<ComponentData>> components_;
    std::vector<Index> component_ids_; // 运行期权威：该 model 拥有哪些 component_id
    

    friend class ModelOperator; //!< 声明 ModelOperator 为友元，以允许其访问 ModelData 私有数据
};
#endif // MODEL_H