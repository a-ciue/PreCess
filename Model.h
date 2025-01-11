#ifndef MODEL_H
#define MODEL_H
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <array>
#include <QObject>
#include <qqmlintegration.h>
#include <qstring.h>

#include "ModelActor.h"

namespace MeshLib {
template <typename V, typename E, typename F, typename H>
class CToolMesh;
class CToolVertex;
class CToolEdge;
class CToolFace;
class CToolHalfEdge;
typedef CToolMesh<CToolVertex, CToolEdge, CToolFace, CToolHalfEdge> CTMesh;
}

struct Patch {
    // patch id
    int id_ { -1 };
    int blockID { -1 };
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
};
struct Block {
    std::unordered_set<int> patchIDs;
    int id;
    int groupID;
};

struct Group {
    std::unordered_set<int> blockIDs;
    int id;
};

//! @brief Model主要负责处理模型数据，先更新模型数据，再更新ModelActor调函数
class Model :public QObject {
    Q_OBJECT
    QML_ELEMENT

public:
    //! @brief 根据给定CTMesh构造update_patches, blocks_, groups_，actor_构造函数
    Model(std::unique_ptr<MeshLib::CTMesh> mesh);

    void refreshVtk();

    //! @brief 输出网格文件，选择面输出（不带组信息）、块输出、组输出
    //! @param mesh_path 输出文件路径
    //! @param mode 选定输出模式
    //! @param extension 输出文件拓展名
    void write_mesh(const std::filesystem::path& mesh_path, ModelActor::RenderMode mode, const QString &extension);

    //! @brief 根据给定id找到mesh的face，进行面分割
    //! @param patch_id 面所在的patch
    //! @param face_id 在该patch上的face id
    Q_INVOKABLE void split_face(int patch_id, int face_id);
    //! @brief 根据给定id找到mesh的edge，进行边分割
    //! @param patch_id 边所在的patch
    //! @param edge_v_id1 其中一个边点id
    //! @param edge_v_id2 另一个边点id
    Q_INVOKABLE void split_edge(int patch_id, int edge_v_id1, int edge_v_id2);

    //! @brief 合并给定block，并更新block actor，依赖ModelActor
    //! @param block_ids
    Q_INVOKABLE void merge_blocks(const std::vector<int>& block_ids);
    //! @brief 合并给定group，并更新group actor，依赖ModelActor
    //! @param group_ids
    Q_INVOKABLE void merge_groups(const std::vector<int>& group_ids);

    //! @brief remesh指定block，依赖MeshUtil、update_patches、update_actors
    Q_INVOKABLE void remesh_block(const std::vector<int>& block_ids);
    //! @brief remesh指定group，依赖MeshUtil、update_patches、update_actors
    Q_INVOKABLE void remesh_group(const std::vector<int>& group_id);

    int face_patch_id(int face_id);
    const std::vector<int>& patch_face_ids(int patch_id);
    const std::vector<int>& patch_vertex_ids(int patch_id);
    int patch_block_id(int patch_id);
    //const std::vector<int>& block_patch_ids(int block_id);
    int block_group_id(int patch_id);
    //const std::vector<int>& group_block_ids(int group_id);

signals:
    void modelInited(const std::unordered_map<int, std::unique_ptr<Patch>>* patches,
        const std::unordered_map<int, std::unique_ptr<Block>>* blocks,
        const std::unordered_map<int, std::unique_ptr<Group>>* groups);
    void patchUpdated(int patch_id, const std::vector<std::array<double, 3>>& points, const std::vector<std::array<int, 3>>& triangles);
    void blockUpdated(int block_id, const std::unordered_set<int>& block_patches);
    void groupUpdated(int group_id, const std::unordered_set<int>& group_blocks);

    void blocksMerged(const std::vector<int>& block_ids, int father_block, const std::unordered_set<int>& father_block_patches);
    void groupMerged(const std::vector<int>& group_ids, int father_group, const std::unordered_set<int>& father_group_blocks);

private:
    //! @brief 根据CToolFace::m_g()为面所在patch，读取mesh_更新指定patch的patches
    void update_patches(const std::vector<int>& patch_ids, bool new_patch = true);
    void update_patches(const std::unordered_set<int>& patch_ids, bool new_patch = true);

    //! @brief 更新指定patch的actor
    void update_actors(const std::vector<int>& patch_ids);

    using PatchMap = std::unordered_map<int, std::unique_ptr<Patch>>;
    using BlockMap = std::unordered_map<int, std::unique_ptr<Block>>;
    using GroupMap = std::unordered_map<int, std::unique_ptr<Group>>;

    std::unique_ptr<MeshLib::CTMesh> mesh_;
    PatchMap patches_;
    BlockMap blocks_;
    GroupMap groups_;
};
#endif // MODEL_H
