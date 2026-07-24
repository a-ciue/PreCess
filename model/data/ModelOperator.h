#pragma once
#include "ModelOperatorBase.h"
#include "ModelData.h"

#include <memory>
#include <filesystem>

class ModelObserver;
class ModelLayer;
class QSelection; // 前向声明选择类
/**
 * @brief ModelOperator 模型对象基类
 *
 * ModelOperator 是模型基类。
 * 持有一个 ModelData 指针，以及一个可选的模型观察者，用于在模型数据发生更改时通知外部。
 * 通过 ModelOperator，可以对模型数据执行修改操作（通常通过各具体 ICommand 子类实现），并在操作后通知观察者以更新界面等。
 */
class ModelOperator : public ModelOperatorBase {
public:
    /**
     * @brief 构造 ModelOperator 对象
     * @param modelData 关联的模型数据指针
     * @param manager 模型数据管理器，用于维护全局 Component 数据。
     * @param observer 关联的模型观察者指针（可选），用于在模型更改时发出通知
     */
    ModelOperator(Index id, ModelData& modelData, ModelLayer& manager, ModelObserver* observer = nullptr)
        : id_(id)
        , model_(&modelData)
        , manager_(&manager)
        , observer_(observer) { }

    /**
     * @brief 获取关联的模型数据
     * @return 指向 ModelData 的指针
     */
    ModelData& data() const;

    /**
     * @brief 获取关联的模型观察者
     * @return 指向 QModelObserver 的指针（如果有）
     */
    ModelObserver* observer() const;

    //! @brief 根据给定id找到mesh的edge，进行边分割
    //! @param patch_id 边所在的patch
    //! @param edge_v_id1 其中一个边点id
    //! @param edge_v_id2 另一个边点id
    void notifyChanged();

    /**
     * @brief 向当前模型增加一个几何组件，并初始化几何子形状索引。
     * @param component 包含 GeometryData 的组件。
     * @return 新组件的全局 ID。
     */
    Index addGeometryComponent(std::unique_ptr<ComponentData> component);

    //! @brief 合并给定group，并更新group actor，依赖ModelActor
    //! @param group_ids
    // void merge_groups(QSelection* selection);

    //! @brief remesh指定group，依赖MeshUtil、update_patches、update_actors
    // void remesh_group(QSelection* selection);

    Index getId() const override;

private:
    Index id_;
    ModelData* model_; //!< 被操作的模型数据指针
    ModelLayer* manager_; //!< 模型数据管理器，用于维护全局 Component 数据
    ModelObserver* observer_; //!< 模型观察者指针，用于通知外部变化（可为 nullptr）
};
