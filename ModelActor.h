#ifndef MODELACTOR_H
#define MODELACTOR_H
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <vtkNew.h>
#include <vtkAssembly.h>

struct Group;
struct Block;
struct Patch;
class vtkActor;
class vtkRenderer;
class vtkPolyData;
class Model;

class Model;
//! @brief 负责管理Model的Actor
class ModelActor {
    friend class Model;

public:
    enum class RenderMode {
        Face,
        Block,
        Group
    };

    ModelActor(
        const std::unordered_map<int, std::unique_ptr<Patch>>& patches,
        const std::unordered_map<int, std::unique_ptr<Block>>& blocks,
        const std::unordered_map<int, std::unique_ptr<Group>>& groups);
    //! @brief 从renderer中解除对应actor的绑定
    ~ModelActor();

    void bind_renderer(vtkRenderer* renderer);
    //! @brief 设置某个renderer是否渲染边
    //! @param mode 选择该mode对应renderer
    //! @param render 是否渲染
    void render_edge(RenderMode mode, bool render);

    int block_actor_id(vtkActor* actor);
    int group_actor_id(vtkActor* actor);
    int patch_actor_id(vtkActor* actor);

    int patch_global_fid(int patch_id, int local_fid);
    int patch_global_vid(int patch_id, int local_vid);

    vtkNew<vtkAssembly> face_assembly_;
    vtkNew<vtkAssembly> block_assembly_;
    vtkNew<vtkAssembly> group_assembly_;

    //! @brief 合并给定id的block的Actor，并删除被合并的Actor
    //! @param block_ids 要合并的block
    //! @param father_block 留下的block
    //! @param father_block_patches 留下的block合并后拥有的patches id
    void merge_blocks(const std::vector<int>& block_ids, int father_block, const std::unordered_set<int>& father_block_patches);
    //! @brief 合并给定id的group的Actor，并删除被合并的Actor
    //! @param group_ids 要合并的group
    //! @param father_group 留下的group
    //! @param father_group_blocks 留下的group合并后拥有的blocks id
    void merge_groups(const std::vector<int>& group_ids, int father_group, const std::unordered_set<int>& father_group_blocks);

    //! @brief 根据输入数据更新patch的mapper
    //! @param patch_id 要更新的patch
    //! @param points 坐标数据
    //! @param triangles 三角形索引数组
    void update_patch(int patch_id, const std::vector<std::array<double, 3>>& points, const std::vector<std::array<int, 3>>& triangles);
    //! @brief 更新指定block的actor的mapper
    void update_block(int block_id, const std::unordered_set<int>& block_patches);
    //! @brief 更新指定group的actor的mapper
    void update_group(int group_id, const std::unordered_set<int>& group_blocks);
    void update_assembly();

private:
    //! @brief 将给定的actors合并到father_actor中
    static void _merge_actors(vtkActor* father_actor, const std::vector<vtkActor*>& actors);

    using ActorMap = std::unordered_map<int, vtkSmartPointer<vtkActor>>;

    // Model* model_;
    const std::unordered_map<int, std::unique_ptr<Patch>>& patches_;

    //vtkRenderer* face_renderer_ {};
    //vtkRenderer* block_renderer_ {};
    //vtkRenderer* group_renderer_ {};

    ActorMap patch_actors_;
    std::unordered_map<vtkActor*, int> patch_actor_id_;
    ActorMap block_actors_;
    std::unordered_map<vtkActor*, int> block_actor_id_;
    ActorMap group_actors_;
    std::unordered_map<vtkActor*, int> group_actor_id_;

    std::unordered_map<RenderMode, bool> edge_visibility;
};

#endif // MODELACTOR_H
