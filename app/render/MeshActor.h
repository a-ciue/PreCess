#ifndef MESH_ACTOR_H
#define MESH_ACTOR_H
#include "Core.h"
#include <vtkPropCollection.h>
#include <vtkMinimalStandardRandomSequence.h>
#include <vtkNamedColors.h>
#include <vtkCompositePolyDataMapper.h>
#include <vtkActor.h>
class vtkUnstructuredGrid;
class vtkRenderer;
class MeshActorSelectOp;

//! @brief 负责管理Model的Actor
class MeshActor {
    friend MeshActorSelectOp;

public:
    static vtkNew<vtkMinimalStandardRandomSequence> randomSequence;
    static vtkNew<vtkNamedColors> colors;

    MeshActor(vtkRenderer* renderer, bool is_edge_render, ModelRenderMode render_mode);
    ~MeshActor();

    void loadModelData(const MeshDataVtk& model_data);

    void setVisibility(bool visibility);
    void setRenderEdge(bool is_render);
    void setRenderMode(ModelRenderMode render_mode);

    bool getIsEdgeRender();
    ModelRenderMode getMeshRenderMode();

private:
    ModelRenderMode render_mode_;
    bool edge_render_{ true };
    bool visibility_{ true };
    std::unique_ptr<MeshDataVtk> model_data_;

    vtkNew<vtkActor> solid_actor_;
    vtkNew<vtkActor> face_actor_;
    vtkNew<vtkActor> edge_actor_;

    vtkNew<vtkUnstructuredGrid> solid_data_;
    vtkNew<vtkPolyData> face_data_;
    vtkNew<vtkPolyData> edge_data_;

    vtkRenderer* renderer_;

    vtkNew<vtkActor> actor_;
    //Face mapper
    vtkNew<vtkCompositePolyDataMapper> block_mapper_;
    void createBlockMapper(const MeshDataVtk& model_data);
    vtkSmartPointer<vtkUnstructuredGrid> _createSolidUGird(const MeshDataVtk& model_data);
};
#endif // MODEL_ACTOR_H
