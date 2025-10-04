#include "MeshActorSelectOp.h"
#include "MeshActor.h"

#include <vtkSelection.h>
#include <vtkSelectionNode.h>
#include <vtkUnstructuredGrid.h>

MeshActorSelectOpFactory::MeshActorSelectOpFactory() = default;
MeshActorSelectOpFactory::MeshActorSelectOpFactory(std::weak_ptr<const MeshActor> mesh_actor)
    : mesh_actor_(mesh_actor)
{
}

std::optional<MeshActorSelectOp> MeshActorSelectOpFactory::lock()
{
    if (auto mesh_actor = mesh_actor_.lock()) {
        return { mesh_actor };
    }
    return {};
}

MeshActorSelectOp::MeshActorSelectOp(std::shared_ptr<const MeshActor> mesh_actor)
    : mesh_actor_(mesh_actor)
{
    if (!mesh_actor_) {
        throw std::runtime_error("MeshActorSelectOp: mesh_actor is nullptr");
    }
}

vtkProp& MeshActorSelectOp::getSolidActor()
{
    return *mesh_actor_->solid_actor_;
}

vtkProp& MeshActorSelectOp::getFaceActor()
{
    return *mesh_actor_->face_actor_;
}

vtkProp& MeshActorSelectOp::getEdgeActor()
{
    return *mesh_actor_->edge_actor_;
}

vtkProp& MeshActorSelectOp::getBlockActor()
{
    return *mesh_actor_->actor_;
}

vtkSmartPointer<vtkExtractSelection> MeshActorSelectOp::extractSolid(vtkIdTypeArray* ids)
{
    vtkNew<vtkSelectionNode> selectionNode;
    selectionNode->SetFieldType(vtkSelectionNode::CELL);
    selectionNode->SetContentType(vtkSelectionNode::INDICES);
    selectionNode->SetSelectionList(ids);

    vtkNew<vtkSelection> selection;
    selection->SetNode("s", selectionNode);

    vtkNew<vtkExtractSelection> extractSelection;
    extractSelection->SetInputData(0, mesh_actor_->solid_data_.GetPointer());
    extractSelection->SetInputData(1, selection);

    return extractSelection;
}
