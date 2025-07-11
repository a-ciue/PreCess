/**
 * @file ModelManager.h
 * @brief 负责管理多个模型实例的类
 *
 * ModelManager 仅负责管理模型（添加、删除、查询、重命名）以及模型事件的发送。
 * 与文件 IO、样条转换相关的功能已全部移至 FileHandler。
 * 但原接口仍然使用。
 *
 * @author 徐昊阳 haoyangxu06@gmail.com
 * @date 2025/3/20
 */
#ifndef MODEL_MANAGER_H
#define MODEL_MANAGER_H
#include "ModelData.h"

#include "ModelOperator.h"

class QModelObserver;  // 前向声明模型观察者类
class QModelQuery;      // 前向声明 QModelQuery 类

/**
 * @brief 负责管理多个 ModelData 实例的类
 *
 * ModelManager 允许动态添加、删除和查找模型，通过Observer模式发送模型事件。
 * 使得 QML 层能够访问和控制网格数据。
 */
class ModelManager {

public:
    /**
     * @brief 构造 ModelManager 对象
     *
     * @param parent 父对象，默认为 nullptr
    * @param observer 模型观察者对象，用于捕获模型事件（默认 nullptr）
     */
    explicit ModelManager(QModelObserver* observer = nullptr) :  observer_(observer) {}

    /**
     * @brief 添加一个模型
     *
     * @param model_name 新模型的名称
     * @param model 需要添加的模型对象
     */
    Index addModel(std::unique_ptr<ModelData> model);

    /**
     * @brief 移除指定名称的模型
     *
     * @param model_id 需要移除的模型 ID
     */
    void removeModel(Index model_id);

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

    ModelData* getModel(Index model_id) const;
private:

    std::unordered_map<Index, std::unique_ptr<ModelData>> models_;
    Index max_index_{ -1 }; //!< 最大索引值，用于唯一标识模型
    QModelObserver* observer_{ nullptr };                     //!< 全局模型观察者，用于捕获模型事件

    friend class QModelQuery;
};
#endif // MODEL_MANAGER_H
