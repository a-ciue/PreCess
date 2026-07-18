#include "MeshActorSelectOp.h"
#include "MeshActor.h"

#include <vtkExtractSelection.h>
#include <vtkLine.h>
#include <vtkPolyData.h>
#include <vtkSelection.h>
#include <vtkSelectionNode.h>
#include <vtkUnstructuredGrid.h>

MeshActorSelectOpFactory::MeshActorSelectOpFactory() = default;
MeshActorSelectOpFactory::MeshActorSelectOpFactory(std::weak_ptr<MeshActor> mesh_actor)
    : mesh_actor_(mesh_actor)
{
}

std::optional<MeshActorSelectOp> MeshActorSelectOpFactory::lock()
{
    if (auto mesh_actor = mesh_actor_.lock()) {
        return { mesh_actor };
    }
    return { };
}

MeshActorSelectOp::MeshActorSelectOp(std::shared_ptr<MeshActor> mesh_actor)
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

bool MeshActorSelectOp::isVisible() const
{
    return mesh_actor_->isVisible();
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

vtkSmartPointer<vtkExtractSelection> MeshActorSelectOp::extractVertex(vtkIdTypeArray* ids)
{
    vtkNew<vtkSelectionNode> selectionNode;
    selectionNode->SetFieldType(vtkSelectionNode::POINT); // 从 points 中选取点
    selectionNode->SetContentType(vtkSelectionNode::INDICES);
    selectionNode->SetSelectionList(ids);

    vtkNew<vtkSelection> selection;
    selection->SetNode("v", selectionNode);

    vtkNew<vtkExtractSelection> extractSelection;
    extractSelection->SetInputData(0, mesh_actor_->solid_data_.GetPointer()); // 因为点数据 vertex_positions_ 是被所有数据共享的，这里谁都行
    extractSelection->SetInputData(1, selection);

    return extractSelection;
}

vtkSmartPointer<vtkPolyData> MeshActorSelectOp::extractEdge(std::vector<std::array<vtkIdType, 2>>& ids)
{
    // 创建包含选中边的PolyData
    vtkNew<vtkPolyData> edgePolyData;
    vtkNew<vtkPoints> points;
    vtkNew<vtkCellArray> lines;

    // 获取原始点数据
    vtkPoints* originalPoints = mesh_actor_->solid_data_->GetPoints();
    if (!originalPoints) {
        return nullptr;
    }

    // 添加选中的边
    vtkIdType numIds = ids.size();
    for (vtkIdType i = 0; i < numIds; i++) {
        vtkIdType ptId1 = ids.at(i)[0];
        vtkIdType ptId2 = ids.at(i)[1];

        // 检查点ID是否有效
        if (ptId1 >= 0 && ptId1 < originalPoints->GetNumberOfPoints() && ptId2 >= 0 && ptId2 < originalPoints->GetNumberOfPoints()) {

            // 获取原始点坐标
            double pt1[3], pt2[3];
            originalPoints->GetPoint(ptId1, pt1);
            originalPoints->GetPoint(ptId2, pt2);

            // 添加点
            vtkIdType newPtId1 = points->InsertNextPoint(pt1);
            vtkIdType newPtId2 = points->InsertNextPoint(pt2);

            // 创建线单元
            vtkNew<vtkLine> line;
            line->GetPointIds()->SetId(0, newPtId1);
            line->GetPointIds()->SetId(1, newPtId2);
            lines->InsertNextCell(line);
        }
    }

    edgePolyData->SetPoints(points);
    edgePolyData->SetLines(lines);

    return edgePolyData;
}