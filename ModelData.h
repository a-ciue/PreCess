/**
 * @file ModelData.h
 * @brief 负责管理和操作网格模型数据的核心类
 *
 * ModelData 类用于存储和处理网格模型数据，包括面（Patch）、块（Block）和组（Group）的管理。
 * 它提供了一系列函数用于网格操作，如网格划分、合并和重划分等，同时维护与 ModelActor 之间的关联，
 * 以便进行可视化和渲染。
 *
 * @author 徐昊阳 haoyangxu06@gmail.com
 * @date 2025/3/8
 */
#ifndef MODEL_H
#define MODEL_H
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <array>
#include <QObject>
#include <qqmlintegration.h>
#include <qstring.h>

#include "Selection.h"
#include "ToolMesh.h"
#include "Core.h"

namespace MeshLib {
template <typename V, typename E, typename F, typename H>
class CToolMesh;
class CToolVertex;
class CToolEdge;
class CToolFace;
class CToolHalfEdge;
typedef CToolMesh<CToolVertex, CToolEdge, CToolFace, CToolHalfEdge> CTMesh;
}

/**
 * @brief 表示网格中的一个 Patch
 *
 * Patch 由多个三角形面组成，并包含其在全局网格中的 ID 信息
 * 同时Patch为模型的自身属性，不随相关操作而更改
 */
struct Patch {
    // patch id
    int id_ { -1 };
    int blockID { -1 };

    // 新增的父节点id字段
    int father_id{ -1 }; // 默认值为-1，表示没有父节点

    // 全局id
    std::vector<int> faceIDs_;
    // 三角形的局部id索引
    std::vector <std::array<int, 3>> faceTriangles_;
    //std::vector<int[3]> faceTriangles_;

    // 全局id
    std::vector<int> vertexIDs_;
    // 坐标
    std::vector <std::array<double, 3>> vertexPoints_;
    //std::vector<double[3]> vertexPoints_;

    // 构造函数
    Patch() = default;
    Patch(int id, int block) : id_(id), blockID(block) {}
};

/**
 * @brief 表示网格中的一个 Block（块）
 *
 * Block 由多个 Patch 组成，具有唯一 ID，并归属于某个 Group。
 */
struct Block {
    std::unordered_set<int> patchIDs;
    int id;
    int groupID;
};

/**
 * @brief 表示网格中的一个 Block（块）
 *
 * Block 由多个 Patch 组成，具有唯一 ID，并归属于某个 Group。
 */
struct Group {
    std::unordered_set<int> blockIDs;
    int id;
};

using PatchMap = std::unordered_map<int, std::unique_ptr<Patch>>;
using BlockMap = std::unordered_map<int, std::unique_ptr<Block>>;
using GroupMap = std::unordered_map<int, std::unique_ptr<Group>>;

//! @brief Model主要负责处理模型数据，先更新模型数据，再更新ModelActor调函数
/**
*@brief 负责管理和操作网格模型数据的核心类
*
* ModelData 负责管理网格数据，包括 Patch、Block 和 Group 的存储、更新和操作。
* 该类提供了网格划分、合并、重划分等功能，并维护与 ModelActor 之间的关联，
* 以便进行可视化和渲染。
*/
class ModelData : public QObject {
    Q_OBJECT
    QML_ELEMENT

public:
    /**
     * @brief 构造 ModelData 对象
     *
     * 该构造函数基于传入的 CTMesh 对象初始化模型的 patches、blocks、groups 以及 ModelActor。
     *
     * @param mesh 指向 CTMesh 的智能指针，表示网格数据
     */
    ModelData(std::unique_ptr<MeshLib::CTMesh> mesh);

    /**
     * @brief 刷新 VTK 渲染数据
     *
     * 该函数用于刷新 VTK 相关数据，使模型的可视化状态与当前数据保持同步。
     */
    void refreshVtk();

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
    
    /**
     * @brief 获取指定面 (face) 所属的 patch ID
     *
     * @param face_id 需要查询的面 ID
     * @return int 该面所属的 patch ID
     */
    int face_patch_id(int face_id);

    /**
     * @brief 获取指定 patch 内所有面的 ID
     *
     * @param patch_id 需要查询的 patch ID
     * @return const std::vector<int>& 该 patch 内包含的所有面 ID
     */
    const std::vector<int>& patch_face_ids(int patch_id);

    /**
     * @brief 获取指定 patch 内所有顶点的 ID
     *
     * @param patch_id 需要查询的 patch ID
     * @return const std::vector<int>& 该 patch 内包含的所有顶点 ID
     */
    const std::vector<int>& patch_vertex_ids(int patch_id);

    /**
     * @brief 获取指定 patch 所属的 block ID
     *
     * @param patch_id 需要查询的 patch ID
     * @return int 该 patch 所属的 block ID
     */
    int patch_block_id(int patch_id);
    //const std::vector<int>& block_patch_ids(int block_id);

    /**
     * @brief 获取指定 block 所属的 group ID
     *
     * @param patch_id 需要查询的 patch ID
     * @return int 该 block 所属的 group ID
     */
    int block_group_id(int patch_id);
    //const std::vector<int>& group_block_ids(int group_id);

    /**
     * @brief 获取模型名称
     *
     * @return QString 当前模型的名称
     */
    QString getModelName() const { return model_name; }

    /**
     * @brief 设置模型名称
     *
     * @param name 要设置的模型名称
     */
    void setModelName(const QString& name) { model_name = name; }

    // 只读访问接口
    const MeshLib::CTMesh* mesh() const;
    const PatchMap& patches() const;
    const BlockMap& blocks() const;
    const GroupMap& groups() const;

signals:
    /**
     * @brief 当模型初始化完成时触发
     *
     * 该信号在模型数据加载完成后被触发，通知外部组件模型的基本数据已经准备就绪。
     *
     * @param modelName 模型的名称
     * @param patches 模型中的 Patch 数据指针
     * @param blocks 模型中的 Block 数据指针
     * @param groups 模型中的 Group 数据指针
     */
    void modelInited(const QString& modelName,
        const std::unordered_map<int, std::unique_ptr<Patch>>* patches,
        const std::unordered_map<int, std::unique_ptr<Block>>* blocks,
        const std::unordered_map<int, std::unique_ptr<Group>>* groups);

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



    //! @brief 根据CToolFace::m_g()为面所在patch，读取mesh_更新指定patch的patches
    void update_patches(const std::vector<int>& patch_ids, bool new_patch = true);
    void update_patches(const std::unordered_set<int>& patch_ids, bool new_patch = true);

    //! @brief 更新指定patch的actor
    void update_actors(const std::vector<int>& patch_ids);

    //! @brief 更新指定patch的father id
    void update_father_id(int patch_id, int father_id);

private:


    std::unique_ptr<MeshLib::CTMesh> mesh_;
    QString model_name;
    PatchMap patches_;
    BlockMap blocks_;
    GroupMap groups_;

    friend class ModelQuery;          //!< 声明 ModelQuery 为友元，以允许其访问 ModelData 私有数据
    friend class TestModel;           //!< 声明 TestModel 为友元，用于GoogleTest
};
#endif // MODEL_H