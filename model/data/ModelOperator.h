#pragma once
#include "ModelOperatorBase.h"
#include "ModelData.h"

#include <memory>
#include <filesystem>

class ModelLayer;
/**
 * @brief ModelOperator 模型对象基类
 *
 * ModelOperator 是模型基类。
 * 持有一个 ModelData 指针；模型/组件结构变更的通知经 ModelLayer 持有的观察者发出
 * （结构操作保持即时通知，不进待通知集合）。
 * 通过 ModelOperator，可以对模型数据执行修改操作，并在操作后通知观察者以更新界面等。
 */
class ModelOperator : public ModelOperatorBase {
public:
    /**
     * @brief 构造 ModelOperator 对象
     * @param modelData 关联的模型数据指针
     * @param manager 模型数据管理器，用于维护全局 Component 数据（观察者由其持有）
     */
    ModelOperator(Index id, ModelData& modelData, ModelLayer& manager)
        : id_(id)
        , model_(&modelData)
        , manager_(&manager) { }

    /**
     * @brief 获取关联的模型数据
     * @return 指向 ModelData 的指针
     */
    ModelData& data() const;

    //! @brief 通知观察者当前模型已更改（notifyModelChanged）
    void notifyChanged();

    /**
     * @brief 向当前模型增加一个几何组件，并初始化几何子形状索引。
     * @param component 包含 GeometryData 的组件。
     * @return 新组件的全局 ID。
     */
    Index addGeometryComponent(std::unique_ptr<ComponentData> component);

    Index getId() const override;

private:
    Index id_;
    ModelData* model_; //!< 被操作的模型数据指针
    ModelLayer* manager_; //!< 模型数据管理器，用于维护全局 Component 数据
};
