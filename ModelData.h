/**
 * @file ModelData.h
 * @brief 负责管理和操作网格模型数据的核心类
 *
 * ModelData 类用于存储和处理网格模型数据，包括面（Patch）、块（Block）和组（Group）的管理。
 * 它提供了一系列函数用于网格操作，如网格划分、合并和重划分等，同时维护与 MeshActor 之间的关联，
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

//! @brief Model主要负责处理模型数据，先更新模型数据，再更新ModelActor调函数
/**
*@brief 负责管理和操作网格模型数据的核心类
*
* ModelData 负责管理网格数据，包括 Patch、Block 和 Group 的存储、更新和操作。
* 该类提供了网格划分、合并、重划分等功能，并维护与 MeshActor 之间的关联，
* 以便进行可视化和渲染。
*/
class ModelData {
public:
    /**
     * @brief 构造 ModelData 对象
     *
     * 该构造函数基于传入的 CTMesh 对象初始化模型的 patches、blocks、groups 以及 MeshActor。
     *
     * @param mesh 指向 CTMesh 的智能指针，表示网格数据
     */
    ModelData(std::unique_ptr<MeshLib::CTMesh> mesh);

private:
    using PatchMap = std::unordered_map<int, std::unique_ptr<Patch>>;
    using BlockMap = std::unordered_map<int, std::unique_ptr<struct Block>>;
    using GroupMap = std::unordered_map<int, std::unique_ptr<Group>>;

    //! @brief 输出网格文件，选择面输出（不带组信息）、块输出、组输出
    //! @param mesh_path 输出文件路径
    //! @param mode 选定输出模式
    //! @param extension 输出文件拓展名
    void write_mesh(const std::filesystem::path& mesh_path, ModelRenderMode mode, const QString &extension);

    
    //! @brief 根据给定id找到mesh的face，进行面分割
    //! @param patch_id 面所在的patch
    //! @param face_id 在该patch上的face id
    void split_face(QSelection* selection);

    //! @brief 根据给定id找到mesh的edge，进行边分割
    //! @param patch_id 边所在的patch
    //! @param edge_v_id1 其中一个边点id
    //! @param edge_v_id2 另一个边点id
    void split_edge(QSelection* selection);
    
    //! @brief 合并给定block，并更新block actor，依赖ModelActor
    //! @param block_ids
    void merge_blocks(QSelection* selection);

    //! @brief 合并给定group，并更新group actor，依赖ModelActor
    //! @param group_ids
    void merge_groups(QSelection* selection);
    
    //! @brief remesh指定block，依赖MeshUtil、update_patches、update_actors
    void remesh_block(QSelection* selection);

    //! @brief remesh指定group，依赖MeshUtil、update_patches、update_actors
    void remesh_group(QSelection* selection);

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


    Index getId() const { return id_; }

    /**
     * @brief 获取模型名称
     *
     * @return QString 当前模型的名称
     */
    QString getModelName() const { return model_name_; }

    /**
     * @brief 设置模型名称
     *
     * @param name 要设置的模型名称
     */
    void setModelName(const QString& name) { model_name_ = name; }

    /**
     * @brief 获取渲染模型需要的数据
     * @return 模型数据
     */
    MeshDataVtk getModelData();

    //! @brief 根据CToolFace::m_g()为面所在patch，读取mesh_更新指定patch的patches
    void update_patches(const std::vector<int>& patch_ids, bool new_patch = true);
    void update_patches(const std::unordered_set<int>& patch_ids, bool new_patch = true);

    //! @brief 更新指定patch的actor
    void update_actors(const std::vector<int>& patch_ids);

    //! @brief 更新指定patch的father id
    void update_father_id(int patch_id, int father_id);

    std::unique_ptr<MeshLib::CTMesh> mesh_;
    QString model_name_;
    Index id_ { -1 }; //!< 模型的唯一标识符
    PatchMap patches_;
    BlockMap blocks_;
    GroupMap groups_;

    friend class QModelQuery;          //!< 声明 QModelQuery 为友元，以允许其访问 ModelData 私有数据
    friend class TestModel;           //!< 声明 TestModel 为友元，用于GoogleTest
    friend class ModelOperator;    //!< 声明 ModelOperator 为友元，以允许其访问 ModelData 私有数据
    friend class FileHandler; //!< 声明 FileHandler 为友元，以允许其访问 ModelData 私有数据
    friend class ModelManager; //!< 声明 ModelManager 为友元，以允许其访问 ModelData 私有数据
};
#endif // MODEL_H