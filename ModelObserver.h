#pragma once
#include <QObject>
#include <qqmlintegration.h>
#include <unordered_set>

/**
 * @brief QModelObserver：用于监听模型数据更改并向外通知的观察者类
 *
 * QModelObserver 通过槽函数接收模型层发出的事件，然后通过信号将这些事件通知给外部（例如 UI 层）。
 * 这样可以将模型数据的变化与界面解耦，当模型发生变化时，观察者发出相应的信号供 QML 或其他组件处理。
 */
class QModelObserver : public QObject {
    Q_OBJECT
	QML_ELEMENT
public:
    /**
     * @brief 接收模型数据变更的通知函数
     * @param modelName 发生变化的模型名称
     *
     * 该槽函数由模型层在模型数据发生更改时调用，以便触发相应的信号通知。
     */
    void notifyModelChanged(const QString& modelName) {
        emit modelChanged(modelName);
    }

    /**
     * @brief 接收模型被添加的通知函数 
     * @param modelName 添加的模型名称
     *
     * 该通知函数用于在模型被导入到软件中时发出信号，通知外部组件。
     */
    void notifyModelAdded(const QString& modelName) {
        emit modelAdded(modelName);
    }

	/**
     * @brief 接收模型被移除的通知函数
     * @param modelName 移除的模型名称
     *
     * 该通知函数用于在模型被从软件中移除时发出信号，通知外部组件。
     */
    void notifyModelRemoved(const QString& modelName)
    {
        emit modelRemoved(modelName);
    }

    /**
     * @brief 接收模型名称变更的通知函数
     * @param oldName 旧模型名称
     * @param newName 新模型名称
     *
     * 该通知函数用于在模型名称发生变化时发出信号，通知外部组件。
     */
    void notifyModelNameChanged(const QString& oldName, const QString& newName)
    {
        emit modelNameChanged(oldName, newName);
    }

    /**
     * @brief 接收样条曲线加载失败的通知函数
     * @param message 失败信息
     *
     * 该通知函数用于在样条曲线加载失败时发出信号，通知外部组件。
     */
    void notifySplineLoadFailed(const QString& message)
    {
        emit splineLoadFailed(message);
    }



signals:
    /**
     * @brief 样条曲线加载失败信号
     *
     * @param message 失败信息
     */
    void splineLoadFailed(QString message);

    /**
     * @brief 模型名称变更信号
     *
     * @param oldName 旧模型名称
     * @param newName 新模型名称
     */
    void modelNameChanged(const QString& oldName, const QString& newName);
    
    /**
     * @brief 新模型添加信号
     *
     * @param modelName 添加的模型名称
     */
    void modelAdded(const QString& modelName);
    
    /**
     * @brief 模型移除信号
     *
     * @param modelName 移除的模型名称
     */
    void modelRemoved(const QString& modelName);

#pragma region ModelDataSignals

    /**
     * @brief 模型数据发生变化时发出的信号
     * @param modelName 发生变化的模型名称
     */
    void modelChanged(const QString& modelName);
    
    /**
     * @brief 当 Patch 更新时触发
     *
     * 该信号用于通知外部组件某个 Patch 的顶点坐标或三角形索引发生了变化。
     *
     * @param modelName 模型的名称
     * @param patch_id 发生变化的 Patch ID
     * @param points Patch 内顶点的新坐标
     * @param triangles Patch 内三角形的新索引
     */
    void patchUpdated(const QString& modelName, int patch_id,
        const std::vector<std::array<double, 3>>& points,
        const std::vector<std::array<int, 3>>& triangles);

    /**
     * @brief 当 Block 更新时触发
     *
     * 该信号用于通知外部组件某个 Block 发生了变化，例如其包含的 Patch 发生调整。
     *
     * @param modelName 模型的名称
     * @param block_id 发生变化的 Block ID
     * @param block_patches Block 内包含的 Patch ID 集合
     */
    void blockUpdated(const QString& modelName, int block_id,
        const std::unordered_set<int>& block_patches);

    /**
     * @brief 当 Group 更新时触发
     *
     * 该信号用于通知外部组件某个 Group 发生了变化，例如其包含的 Block 发生调整。
     *
     * @param modelName 模型的名称
     * @param group_id 发生变化的 Group ID
     * @param group_blocks Group 内包含的 Block ID 集合
     */
    void groupUpdated(const QString& modelName, int group_id,
        const std::unordered_set<int>& group_blocks);

    /**
     * @brief 当多个 Block 被合并时触发
     *
     * 该信号用于通知外部组件多个 Block 发生合并，并提供合并后的 Block 信息。
     *
     * @param modelName 模型的名称
     * @param block_ids 参与合并的 Block ID 列表
     * @param father_block 合并后的 Block ID
     * @param father_block_patches 合并后 Block 内包含的 Patch ID 集合
     */
    void blocksMerged(const QString& modelName, const std::vector<int>& block_ids,
        int father_block,
        const std::unordered_set<int>& father_block_patches);

    /**
     * @brief 当多个 Group 被合并时触发
     *
     * 该信号用于通知外部组件多个 Group 发生合并，并提供合并后的 Group 信息。
     *
     * @param modelName 模型的名称
     * @param group_ids 参与合并的 Group ID 列表
     * @param father_group 合并后的 Group ID
     * @param father_group_blocks 合并后 Group 内包含的 Block ID 集合
     */
    void groupMerged(const QString& modelName, const std::vector<int>& group_ids,
        int father_group,
        const std::unordered_set<int>& father_group_blocks);
#pragma endregion // ModelData
};
