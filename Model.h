#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace MeshLib {
template <typename V, typename E, typename F, typename H>
class CToolMesh;
class CToolVertex;
class CToolEdge;
class CToolFace;
class CToolHalfEdge;
typedef CToolMesh<CToolVertex, CToolEdge, CToolFace, CToolHalfEdge> CTMesh;
}

class ModelActor;

struct Patch {
    // patch id
    int id_;
    // 全局id
    std::vector<int> faceIDs_;
    // 三角形的局部id索引
    std::vector<int[3]> faceTriangles_;
    // 全局id
    std::vector<int> vertexIDs_;
    // 坐标
    std::vector<double[3]> vertexPoints_;
};

struct Block {
    std::unordered_set<int> patchIDs;
    int id;
};

struct Group {
    std::unordered_set<int> blockIDs;
    int id;
};

//! @brief Model主要负责处理模型数据，先更新模型数据，再更新ModelActor调函数
class Model {
public:
    //! @brief 根据给定CTMesh构造update_patches, blocks_, groups_，actor_构造函数
    Model(std::unique_ptr<MeshLib::CTMesh> mesh);
    ~Model();

    //! @brief 根据给定id找到mesh的face，进行面分割
    //! @param patch_id 面所在的patch
    //! @param face_id 在该patch上的face id
    void split_face(int patch_id, int face_id);
    //! @brief 根据给定id找到mesh的edge，进行边分割
    //! @param patch_id 边所在的patch
    //! @param face_id 在该patch上的边的端点id
    void split_edge(int patch_id, std::pair<int, int> edge_v_ids);

    //! @brief 合并给定block，并更新block actor，依赖ModelActor
    //! @param block_ids
    void merge_blocks(const std::vector<int>& block_ids);
    //! @brief 合并给定group，并更新group actor，依赖ModelActor
    //! @param group_ids
    void merge_groups(const std::vector<int>& group_ids);

    //! @brief remesh指定block，依赖MeshUtil、update_patches、update_actors
    void remesh_block(int block_id);
    //! @brief remesh指定group，依赖MeshUtil、update_patches、update_actors
    void remesh_group(int group_id);

    int face_patch_id(int face_id);
    const std::vector<int>& patch_face_ids(int patch_id);
    const std::vector<int>& patch_vertex_ids(int patch_id);
    int patch_block_id(int patch_id);
    //const std::vector<int>& block_patch_ids(int block_id);
    int block_group_id(int patch_id);
    //const std::vector<int>& group_block_ids(int group_id);

    ModelActor& actor();

private:
    //! @brief 根据CToolFace::m_g()为面所在patch，读取mesh_更新指定patch的patches
    void update_patches(const std::vector<int>& patch_ids);
    void update_patches(const std::unordered_set<int>& patch_ids);

    //! @brief 更新指定patch的actor
    void update_actors(const std::vector<int>& patch_ids);

    using PatchMap = std::unordered_map<int, std::unique_ptr<Patch>>;
    using BlockMap = std::unordered_map<int, std::unique_ptr<Block>>;
    using GroupMap = std::unordered_map<int, std::unique_ptr<Group>>;

    std::unique_ptr<MeshLib::CTMesh> mesh_;
    PatchMap patches_;
    BlockMap blocks_;
    GroupMap groups_;

    std::unique_ptr<ModelActor> actor_;
};
