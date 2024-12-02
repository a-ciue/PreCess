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


private:
    ModelActor() = default;
    ~ModelActor();

    //void set_model(Model* model);

    //! @brief 合并给定id的block的Actor
    //! @param block_ids 
    void merge_blocks(std::vector<int> block_ids);
    void merge_groups(std::vector<int> group_ids);
    void update_patch(int patch_id, const std::vector<double[3]>& points, const std::vector<int[3]>& triangles);
    void update_block(int block_id);
    void update_group(int group_id);

    Model* model_;
    vtkRenderer* face_renderer_ {};
    vtkRenderer* block_renderer_ {};
    vtkRenderer* group_renderer_ {};
    std::unordered_map<int, vtkNew<vtkActor>> patch_actors_;
    std::unordered_map<int, vtkNew<vtkActor>> block_actors_;
    std::unordered_map<int, vtkNew<vtkActor>> group_actors_;

};

#endif // MODELACTOR_H
