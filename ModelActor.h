#ifndef MODELACTOR_H
#define MODELACTOR_H
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <vtkNew.h>
#include <vtkAssembly.h>
#include <vtkPropAssembly.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkCompositePolyDataMapper.h>
struct BlockData;
struct Group;
struct Block;
struct Patch;
class vtkActor;
class vtkRenderer;
class vtkPolyData;
class Model;
using Index = int;

struct BlockDatas {
    std::vector<BlockData> BlockDatas_;
};

struct BlockData {
    std::vector<vtkIdType> faces_;
    vtkIdType vtk_id_;
    Index model_id_;
};

struct ModelData {
    std::vector<std::array<vtkIdType, 3>> vtk_triangles_;
    std::vector<std::array<double, 3>> vtk_points_;
    std::vector<Index> model_face_id_;
    std::vector<Index> model_point_id_;
    BlockDatas model_blocks_;

    Index model_face_id(vtkIdType face_id);
    Index model_point_id(vtkIdType point_id);
    Index model_block_id(vtkIdType block_id);
};





class Model;

class ModelActor {
public:
    enum class RenderMode {
        Face,
        Block,
    };

    ModelActor(vtkRenderer* renderer, bool is_edge_render, RenderMode render_mode);
    ~ModelActor();

    void loadModelData(ModelData model_data);
    void setVisibility(bool visibility);
    void setRenderEdge(bool is_render);
    void setRenderMode(RenderMode render_mode);

    void addPickList(vtkPropCollection* pick_list);

    Index get_model_face_id(vtkIdType face_id);
    Index get_model_point_id(vtkIdType point_id);
    Index get_model_block_id(vtkIdType block_id);

private:
    RenderMode render_mode_;
    bool edge_render_;
    bool visibility_;
    ModelData model_data_;

    vtkNew<vtkActor> actor_;
    vtkRenderer* renderer_;
    vtkNew<vtkPolyDataMapper> mapper_;
    //Face mapper
    vtkNew<vtkCompositePolyDataMapper> block_mapper_;
    void createBlockMapper(ModelData model_data);
};



//! @brief 负责管理Model的Actor
//class ModelActor {
//    friend class Model;
//
//public:
//    enum class RenderMode {
//        Face,
//        Block,
//        Group
//    };
//
//    ModelActor(
//        const std::unordered_map<int, std::unique_ptr<Patch>>& patches,
//        const std::unordered_map<int, std::unique_ptr<Block>>& blocks,
//        const std::unordered_map<int, std::unique_ptr<Group>>& groups);
//    //! @brief 从renderer中解除对应actor的绑定
//    ~ModelActor();
//
//    void bind_renderer(vtkRenderer* renderer);
//    //! @brief 设置某个renderer是否渲染边
//    //! @param mode 选择该mode对应renderer
//    //! @param render 是否渲染
//    void render_edge(RenderMode mode, bool render);
//    ModelActor* getModelActor(vtkPropAssembly* assembly);
//    int block_actor_id(vtkActor* actor);
//    int group_actor_id(vtkActor* actor);
//    int patch_actor_id(vtkActor* actor);
//
//    int patch_global_fid(int patch_id, int local_fid);
//    int patch_global_vid(int patch_id, int local_vid);
//
//
//
//    //! @brief 合并给定id的block的Actor，并删除被合并的Actor
//    //! @param block_ids 要合并的block
//    //! @param father_block 留下的block
//    //! @param father_block_patches 留下的block合并后拥有的patches id
//    void merge_blocks(const std::vector<int>& block_ids, int father_block, const std::unordered_set<int>& father_block_patches);
//    //! @brief 合并给定id的group的Actor，并删除被合并的Actor
//    //! @param group_ids 要合并的group
//    //! @param father_group 留下的group
//    //! @param father_group_blocks 留下的group合并后拥有的blocks id
//    void merge_groups(const std::vector<int>& group_ids, int father_group, const std::unordered_set<int>& father_group_blocks);
//
//    //! @brief 根据输入数据更新patch的mapper
//    //! @param patch_id 要更新的patch
//    //! @param points 坐标数据
//    //! @param triangles 三角形索引数组
//    void update_patch(int patch_id, const std::vector<std::array<double, 3>>& points, const std::vector<std::array<int, 3>>& triangles);
//    //! @brief 更新指定block的actor的mapper
//    void update_block(int block_id, const std::unordered_set<int>& block_patches);
//    //! @brief 更新指定group的actor的mapper
//    void update_group(int group_id, const std::unordered_set<int>& group_blocks);
//    /**
//     * @brief 切换渲染模式时在renderer中改变渲染的assembly
//     *
//     * 加入该渲染模式下的assembly并在renderer中remove掉其他assembly
//     * @param renderMode 渲染模式
//     */
//    void change_mode(std::string renderMode);
//    void set_visibility(bool visibility);
//    std::vector<vtkActor*> get_remove_actor();
//
//private:
//    //! @brief 将给定的actors合并到father_actor中
//    static void _merge_actors(vtkActor* father_actor, const std::vector<vtkActor*>& actors);
//
//    using ActorMap = std::unordered_map<int, vtkSmartPointer<vtkActor>>;
//
//    // Model* model_;
//    const std::unordered_map<int, std::unique_ptr<Patch>>& patches_;
//
//    //vtkRenderer* face_renderer_ {};
//    //vtkRenderer* block_renderer_ {};
//    //vtkRenderer* group_renderer_ {};  
//     
//    vtkNew<vtkPropAssembly> face_assembly_;
//    vtkNew<vtkPropAssembly> block_assembly_;
//    vtkNew<vtkPropAssembly> group_assembly_;
//
//    vtkRenderer* renderer_;
//
//    ActorMap patch_actors_;
//    std::unordered_map<vtkActor*, int> patch_actor_id_;
//    ActorMap block_actors_;
//    std::unordered_map<vtkActor*, int> block_actor_id_;
//    ActorMap group_actors_;
//    std::unordered_map<vtkActor*, int> group_actor_id_;
//    
//    std::unordered_map<RenderMode, bool> edge_visibility;
//    std::vector<vtkActor*> selections_;
//};
//
#endif // MODELACTOR_H
