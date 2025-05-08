#pragma once
#include <memory>
#include <QString>

class ModelData;
class QModelObserver;

/**
 * @brief ModelOperator 类用于封装对单个模型数据的操作接口
 *
 * ModelOperator 持有一个 ModelData 指针，以及一个可选的模型观察者，用于在模型数据发生更改时通知外部。
 * 通过 ModelOperator，可以对模型数据执行修改操作（通常通过各具体 ICommand 子类实现），并在操作后通知观察者以更新界面等。
 */
class ModelOperator {
public:
    /**
     * @brief 构造 ModelOperator 对象
     * @param modelData 关联的模型数据指针
     * @param observer 关联的模型观察者指针（可选），用于在模型更改时发出通知
     */
    ModelOperator(ModelData* modelData, QModelObserver* observer = nullptr)
            : m_model(modelData), m_observer(observer) {}

    /**
     * @brief 获取关联的模型数据
     * @return 指向 ModelData 的指针
     */
    ModelData* data() const { return m_model; }

    /**
     * @brief 获取关联的模型观察者
     * @return 指向 QModelObserver 的指针（如果有）
     */
    QModelObserver* observer() const { return m_observer; }

private:
    ModelData* m_model;              //!< 被操作的模型数据指针
    QModelObserver* m_observer;     //!< 模型观察者指针，用于通知外部变化（可为 nullptr）
};
