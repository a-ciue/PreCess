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
#include "Component.h"
#include "Selection.h"

#include <memory>
#include <optional>
#include <string>
#include <variant>

struct MeshData;
struct SplineData;

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
    Component* createComponent(Index id, const std::string& name);

    // 访问当前模型中的所有组件
    std::vector<std::unique_ptr<Component>>& components();
    const std::vector<std::unique_ptr<Component>>& components() const;

    /* ============ 构造（仅声明） ============ */
    explicit ModelData(std::unique_ptr<MeshData> mesh);
    explicit ModelData(std::unique_ptr<SplineData> spline);

    ModelData(const ModelData& other) = delete;
    ModelData& operator=(const ModelData& other) = delete;
    ModelData(ModelData&& other) noexcept;
    ModelData& operator=(ModelData&& other) noexcept;
    /* ============ 类型查询 ============ */
    Type type() const;
    bool hasMesh()   const noexcept;
    bool hasSpline() const noexcept;

    /* ============ 访问器 ============ */
    MeshData* asMeshData() noexcept;
    const MeshData* asMeshData() const noexcept;

    SplineData* asSplineData() noexcept;
    const SplineData* asSplineData() const noexcept;
private:
    //! @brief 合并给定block，并更新block actor，依赖ModelActor
    //! @param block_ids
    void merge_blocks(Selection selection);

    std::vector<std::unique_ptr<Component>> components_;

    friend class ModelOperator; //!< 声明 ModelOperator 为友元，以允许其访问 ModelData 私有数据
};
#endif // MODEL_H