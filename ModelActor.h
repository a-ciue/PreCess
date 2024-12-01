#ifndef MODELACTOR_H
#define MODELACTOR_H

class Model;
//! @brief 负责管理Model的Actor
class ModelActor {
    friend class Model;

public:
    enum class RenderMode {Face, Block, Group};
    void bind_renderer(vtkRenderer* , RenderMode mode);


private:
    ModelActor(Model& model);
    ~ModelActor();

    //! @brief 合并给定id的block的Actor
    //! @param block_ids 
    void merge_blocks(std::vector<int> block_ids);
    void merge_groups(std::vector<int> group_ids);
    void update_patch(int patch_id, vtkPolyData* data);

    Model& model_;
    vtkRenderer* face_renderer_ {};
    vtkRenderer* block_renderer_ {};
    vtkRenderer* group_renderer_ {};
    std::unordered_map<int, vtkSmartPointer<vtkActor>> patch_actors_;
    std::unordered_map<int, vtkSmartPointer<vtkActor>> block_actors_;
    std::unordered_map<int, vtkSmartPointer<vtkActor>> group_actors_;

};

#endif // MODELACTOR_H
