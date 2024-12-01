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
    int id;
    // 存储全局索引映射，与patch_face_triangles()的下标对应
    vector<int> faceIDs;
    //std::vector<std::tuple<int, int, int>> faceTriangle;
    // 存储全局索引映射，与patch_vertex_points()的下标对应
    vector<int> vertexIDs;
    //vector<CPoint> vertexPoints;
};

struct Block {
    std::unordered_set<int> patchIDs;
    int id;
};

struct Group {
    std::unordered_set<int> blockIDs;
    int id;
};

//! @brief Model主要负责处理模型数据
class Model {
    friend class ModelActor;

public:
    //! @brief 根据给定CTMesh构造
    Model(std::unique_ptr<MeshLib::CTMesh> pMesh);
    ~Model();

    //! @brief 根据给定id找到mesh的face，进行面分割
    //! @param patch_id 面所在的patch
    //! @param face_id 在该patch上的face id
    void split_face(int patch_id, int face_id);
    //! @brief 根据给定id找到mesh的edge，进行边分割
    //! @param patch_id 边所在的patch
    //! @param face_id 在该patch上的边的端点id
    void split_edge(int patch_id, pair<int, int> edge_v_ids);

    //! @brief 合并给定block，并更新block actor，依赖ModelActor
    //! @param block_ids
    void merge_blocks(vector<int> block_ids);
    //! @brief 合并给定group，并更新group actor，依赖ModelActor
    //! @param group_ids
    void merge_groups(vector<int> group_ids);

    //! @brief remesh指定block，依赖MeshIO
    void remesh_block(int block_id);
    //! @brief remesh指定group，依赖MeshIO
    void remesh_group(int group_id);

    int face_patch_id(int face_id);
    const vector<int>& patch_face_ids(int patch_id);
    const vector<int>& patch_vertex_ids(int patch_id);
    int patch_block_id(int patch_id);
    const vector<int>& block_patch_ids(int block_id);
    int block_group_id(int patch_id);
    const vector<int>& group_block_ids(int group_id);

    ModelActor& actor();

private:
    //! @brief 根据mesh更新patches，需要保证patch ID不发生变化
    void update_patches();
    void update_patch(int patch_id);
	std::vector<std::tuple<int, int, int>> patch_face_triangles(int patch_id);
    std::vector<CPoint> patch_vertex_points(int patch_id);

    using PatchMap = std::unordered_map<int, std::unique_ptr<Patch>>;
    using BlockMap = std::unordered_map<int, std::unique_ptr<Block>>;
    using GroupMap = std::unordered_map<int, std::unique_ptr<Group>>;

    std::unique_ptr<MeshLib::CTMesh> m_pMesh;
    PatchMap patches;
    BlockMap blocks;
    GroupMap groups;

    std::unique_ptr<ModelActor> actor_;
};
