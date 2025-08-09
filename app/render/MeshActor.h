#ifndef MESH_ACTOR_H
#define MESH_ACTOR_H
#include "Core.h"
#include <vtkPropCollection.h>
#include <vtkMinimalStandardRandomSequence.h>
#include <vtkNamedColors.h>
#include <vtkCompositePolyDataMapper.h>
#include <vtkActor.h>
class vtkRenderer;

//! @brief 负责管理Model的Actor
class MeshActor {
public:
    static vtkNew<vtkMinimalStandardRandomSequence> randomSequence;
    static vtkNew<vtkNamedColors> colors;

    MeshActor(vtkRenderer* renderer, bool is_edge_render, ModelRenderMode render_mode);

    void loadModelData(const MeshDataVtk& model_data);
    void deleteMeshActor();

    void setVisibility(bool visibility);
    void setRenderEdge(bool is_render);
    void setRenderMode(ModelRenderMode render_mode);

    bool getIsEdgeRender();
    ModelRenderMode getMeshRenderMode();
    void addPickList(vtkPropCollection* pick_list) const;
    
    Index get_model_block_id(vtkIdType block_id) const;

private:
    ModelRenderMode render_mode_;
    bool edge_render_{ true };
    bool visibility_{ true };
    std::unique_ptr<MeshDataVtk> model_data_;

    vtkNew<vtkActor> actor_;
    vtkRenderer* renderer_;
    vtkNew<vtkPolyDataMapper> mapper_;
    //Face mapper
    vtkNew<vtkCompositePolyDataMapper> block_mapper_;
    void createBlockMapper(const MeshDataVtk& model_data);
};



#endif // MODEL_ACTOR_H
