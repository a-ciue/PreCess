#ifndef MODELACTOR_H
#define MODELACTOR_H
#include <unordered_map>
#include <vector>
#include <vtkNew.h>

class vtkActor;
class vtkRenderer;
class vtkPolyData;
class Model;

class Model;
//! @brief 负责管理Model的Actor
class ModelActor {
    friend class Model;

public:
    enum class RenderMode {Face, Block, Group};
    void bind_renderer(vtkRenderer* , RenderMode mode);
    void set_model(Model* model);

    int block_actor_id(vtkActor* actor);
    int group_actor_id(vtkActor* actor);

private:
    ModelActor() = default;
    ~ModelActor();

    //! @brief 合并给定id的block的Actor，并删除被合并的Actor
    //! @param block_ids 要合并的block
    //! @param father_block 留下的block
    void merge_blocks(std::vector<int> block_ids, int father_block);
    //! @brief 合并给定id的group的Actor，并删除被合并的Actor
    //! @param group_ids 要合并的group
    //! @param father_group 留下的group
    void merge_groups(std::vector<int> group_ids, int father_group);

    //! @brief 根据输入数据更新patch的网格数据
    //! @param patch_id 要更新的patch
    //! @param points 坐标数据
    //! @param triangles 三角形索引数组
    void update_patch(int patch_id, const std::vector<double[3]>& points, const std::vector<int[3]>& triangles);
    //! @brief 更新指定block的actor
    void _update_block(int block_id);
    //! @brief 更新指定group的actor
    //! @param group_id 
    void _update_group(int group_id);

    //! @brief 将给定的actors合并到father_actor中
    static void _merge_actors(vtkActor* father_actor, const std::vector<vtkActor*>& actors);

    Model* model_;
    vtkRenderer* face_renderer_ {};
    vtkRenderer* block_renderer_ {};
    vtkRenderer* group_renderer_ {};
    std::unordered_map<int, vtkNew<vtkActor>> patch_actors_;
    std::unordered_map<int, vtkNew<vtkActor>> block_actors_;
    std::unordered_map<vtkActor*, int> block_actor_id_;
    std::unordered_map<int, vtkNew<vtkActor>> group_actors_;
    std::unordered_map<vtkActor*, int> group_actor_id_;

};

#endif // MODELACTOR_H
