#pragma once
#include <memory>
#include "ModelData.h"

class QModelObserver; // 前向声明模型观察者类
class QSelection; // 前向声明选择类
/**
 * @brief ModelOperator 模型对象基类
 *
 * ModelOperator 是模型基类。
 * 持有一个 ModelData 指针，以及一个可选的模型观察者，用于在模型数据发生更改时通知外部。
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
            : model_(modelData), observer_(observer) {}

    /**
     * @brief 获取关联的模型数据
     * @return 指向 ModelData 的指针
     */
    ModelData* data() const { return model_; }

    /**
     * @brief 获取关联的模型观察者
     * @return 指向 QModelObserver 的指针（如果有）
     */
    QModelObserver* observer() const { return observer_; }

    /**
     * @brief 输出样条文件
     * @param spline_path 输出样条文件路径
     * @param extension 输出文件拓展名
     */
    void write_spline(const std::filesystem::path& spline_path);

    //! @brief 根据给定id找到mesh的edge，进行边分割
    //! @param patch_id 边所在的patch
    //! @param edge_v_id1 其中一个边点id
    //! @param edge_v_id2 另一个边点id
    void notifyChanged();
    
    //! @brief 合并给定block，并更新block actor，依赖ModelActor
    //! @param block_ids
    void merge_blocks(QSelection* selection);

    //! @brief 合并给定group，并更新group actor，依赖ModelActor
    //! @param group_ids
    void merge_groups(QSelection* selection);

    //! @brief remesh指定group，依赖MeshUtil、update_patches、update_actors
    void remesh_group(QSelection* selection);

    int getId() const { return model_->id_; }

private:
    ModelData* model_;              //!< 被操作的模型数据指针
    QModelObserver* observer_;     //!< 模型观察者指针，用于通知外部变化（可为 nullptr）
};
