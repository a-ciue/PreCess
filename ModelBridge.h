// ModelBridge.h
#pragma once
#include "ModelData.h"
#include <memory>
#include <QMap>
#include <QString>

/**
 * ModelBridge：数据上下文对象
 * 用于管理多个模型数据，根据模型名称返回对应的 ModelData 指针
 * 注意：ModelBridge 本身只作为数据的容器，不实现任何操作算法，
 * 所有针对数据的具体修改在各个命令模块中实现。
 */
class ModelBridge {
public:
    // 构造时可以不传入数据，之后使用 addModelData 添加多个模型数据
    ModelBridge() = default;

    // 添加一个模型数据
    void addModelData(const QString& modelName, std::shared_ptr<ModelData> data) {
        m_modelMap.insert(modelName, std::move(data));
    }

    // 根据模型名称返回对应的 ModelData 指针
    std::shared_ptr<ModelData> getModelData(const QString& modelName) const {
        return m_modelMap.value(modelName, nullptr);
    }

    // 如果需要，提供对单个模型数据的接口（兼容旧代码）
    std::shared_ptr<ModelData> getData() const {
        // 假设只有一个模型数据时返回之
        return m_modelMap.isEmpty() ? nullptr : m_modelMap.first();
    }
private:
    QMap<QString, std::shared_ptr<ModelData>> m_modelMap;
};
