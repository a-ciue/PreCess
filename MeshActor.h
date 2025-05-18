#ifndef MESH_ACTOR_H
#define MESH_ACTOR_H
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <vtkNew.h>
#include <vtkAssembly.h>
#include <vtkPropAssembly.h>
#include <vtkMinimalStandardRandomSequence.h>
#include <vtkNamedColors.h>
#include "Core.h"

#include <vtkMultiBlockDataSet.h>
#include <vtkCompositePolyDataMapper.h>
struct Group;
struct Block;
struct Patch;
class vtkActor;
class vtkRenderer;
class vtkPolyData;
class Model;

//! @brief 负责管理Model的Actor
class MeshActor {
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
    void addPickList(vtkPropCollection* pick_list) const;
    
    Index get_model_face_id(vtkIdType face_id) const;
    Index get_model_point_id(vtkIdType point_id) const;
    Index get_model_block_id(vtkIdType block_id) const;

private:
    ModelRenderMode render_mode_;
    bool edge_render_;
    bool visibility_;
    MeshDataVtk model_data_;

    vtkNew<vtkActor> actor_;
    vtkRenderer* renderer_;
    vtkNew<vtkPolyDataMapper> mapper_;
    //Face mapper
    vtkNew<vtkCompositePolyDataMapper> block_mapper_;
    void createBlockMapper(const MeshDataVtk& model_data);
};



#endif // MODEL_ACTOR_H
