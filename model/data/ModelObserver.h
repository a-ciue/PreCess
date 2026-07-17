#pragma once
#include "Core.h"
#include <string>

class ModelObserver {
public:
    virtual ~ModelObserver() = default;

    /**
     * @brief 接收模型数据变更的通知函数
     * @param model_id 发生变化的模型 ID 
     *
     * 该槽函数由模型层在模型数据发生更改时调用，以便触发相应的信号通知。
     */
    virtual void notifyModelChanged(Index model_id) = 0;
    virtual void notifyComponentChanged(Index component_id) = 0;
    
    /**
     * @brief 接收模型被添加的通知函数 
     * @param model_id 新添加的模型 ID
     *
     * 该通知函数用于在模型被导入到软件中时发出信号，通知外部组件。
     */
    virtual void notifyModelAdded(Index model_id) = 0;

	/**
     * @brief 接收模型被移除的通知函数
     * @param model_id 移除的模型 ID
     *
     * 该通知函数用于在模型被从软件中移除时发出信号，通知外部组件。
     */
    virtual void notifyModelRemoved(Index model_id) = 0;
    virtual void notifyComponentRemoved(Index component_id) = 0;
    virtual void notifyMeshRemoved(Index component_id) = 0;
    virtual void notifyGeometryRemoved(Index component_id) = 0;

    /**
     * @brief 接收模型名称变更的通知函数
     * @param model_id 模型 ID
     * @param new_name 新模型名称
     *
     * 该通知函数用于在模型名称发生变化时发出信号，通知外部组件。
     */
    virtual void notifyModelNameChanged(Index model_id, const std::string& new_name) = 0;
    
    /**
     * @brief 接收几何曲线加载失败的通知函数
     * @param message 失败信息
     *
     * 该通知函数用于在几何曲线加载失败时发出信号，通知外部组件。
     */
    virtual void notifyGeometryLoadFailed(const std::string& message) = 0;
};
