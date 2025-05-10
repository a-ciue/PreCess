#pragma once
#include <memory>
#include <QString>
#include <qqmlintegration.h>
#include "ModelObserver.h"
#include "ModelData.h"

/**
 * @brief ModelOperator 类用于封装对单个模型数据的操作接口
 *
 * ModelOperator 持有一个 ModelData 指针，以及一个可选的模型观察者，用于在模型数据发生更改时通知外部。
 * 通过 ModelOperator，可以对模型数据执行修改操作（通常通过各具体 ICommand 子类实现），并在操作后通知观察者以更新界面等。
 */
class ModelOperator : public QObject {
    Q_OBJECT
    QML_ELEMENT  // Qt6+: 导出为 QML 可用类型（Qt5 请使用 qmlRegisterType）
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


    //! @brief 输出网格文件，选择面输出（不带组信息）、块输出、组输出
    //! @param mesh_path 输出文件路径
    //! @param mode 选定输出模式
    //! @param extension 输出文件拓展名
    void write_mesh(const std::filesystem::path& mesh_path, RenderMode mode, const QString &extension);

    
    //! @brief 根据给定id找到mesh的face，进行面分割
    //! @param patch_id 面所在的patch
    //! @param face_id 在该patch上的face id
    Q_INVOKABLE void split_face(QSelection* selection);

    //! @brief 根据给定id找到mesh的edge，进行边分割
    //! @param patch_id 边所在的patch
    //! @param edge_v_id1 其中一个边点id
    //! @param edge_v_id2 另一个边点id
    Q_INVOKABLE void split_edge(QSelection* selection);
    
    //! @brief 合并给定block，并更新block actor，依赖ModelActor
    //! @param block_ids
    Q_INVOKABLE void merge_blocks(QSelection* selection);

    //! @brief 合并给定group，并更新group actor，依赖ModelActor
    //! @param group_ids
    Q_INVOKABLE void merge_groups(QSelection* selection);
    
    //! @brief remesh指定block，依赖MeshUtil、update_patches、update_actors
    Q_INVOKABLE void remesh_block(QSelection* selection);

    //! @brief remesh指定group，依赖MeshUtil、update_patches、update_actors
    Q_INVOKABLE void remesh_group(QSelection* selection);

private:
    ModelData* m_model;              //!< 被操作的模型数据指针
    QModelObserver* m_observer;     //!< 模型观察者指针，用于通知外部变化（可为 nullptr）
};
