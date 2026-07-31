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
    // 选择集携带全局点 id，换算为本 actor 的局部点 id（VTK 点索引）后再提取
    vtkNew<vtkIdTypeArray> local_ids;
    local_ids->SetNumberOfComponents(1);
    const auto& reverse = mesh_actor_->local_point_id_by_global_;
    for (vtkIdType i = 0; i < ids->GetNumberOfTuples(); ++i) {
        auto it = reverse.find(static_cast<Index>(ids->GetValue(i)));
        if (it != reverse.end())
            local_ids->InsertNextValue(it->second);
    }

    vtkNew<vtkSelectionNode> selectionNode;
    selectionNode->SetFieldType(vtkSelectionNode::POINT); // 从 points 中选取点
    selectionNode->SetContentType(vtkSelectionNode::INDICES);
    selectionNode->SetSelectionList(local_ids);

    vtkNew<vtkSelection> selection;
    selection->SetNode("v", selectionNode);

    vtkNew<vtkExtractSelection> extractSelection;
    extractSelection->SetInputData(0, mesh_actor_->solid_data_.GetPointer()); // 面/边/体数据共享同一组件私有点集，这里谁都行
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
        // 选择集携带全局点 id，换算为局部点 id 后从组件私有点集取坐标
        const auto& reverse = mesh_actor_->local_point_id_by_global_;
        auto it1 = reverse.find(static_cast<Index>(ids.at(i)[0]));
        auto it2 = reverse.find(static_cast<Index>(ids.at(i)[1]));
        if (it1 == reverse.end() || it2 == reverse.end())
            continue;
        vtkIdType ptId1 = it1->second;
        vtkIdType ptId2 = it2->second;

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