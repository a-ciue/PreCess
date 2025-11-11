#include "MeshActor.h"
#include "MeshActorSelectOp.h"
#include "Selection.h"
#include "SelectorHighlight.h"
#include <optional>
#include <spdlog/spdlog.h>
#include <vtkDataSetMapper.h>
#include <vtkHardwarePicker.h>
#include <vtkLine.h>
#include <vtkPolyData.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

EdgeSelectorHighlight::EdgeSelectorHighlight(vtkRenderer* renderer)
{
    this->selected_actor_ = vtkSmartPointer<vtkActor>::New();
    this->renderer_ = renderer;

    selected_mapper_->SetInputData(vtkPolyData::New());
    selected_actor_->SetMapper(selected_mapper_);
    selected_actor_->GetProperty()->SetColor(MeshActor::colors->GetColor3d("red").GetData());
    selected_actor_->GetProperty()->SetLineWidth(5);

    renderer_->AddActor(selected_actor_);
}

EdgeSelectorHighlight::~EdgeSelectorHighlight()
{
    renderer_->RemoveActor(selected_actor_);
}

void EdgeSelectorHighlight::clear()
{
    _cancel_highlight(selected_mapper_);
    selections_.clear();
}

SelectionVtk EdgeSelectorHighlight::get()
{
    SelectionVtk back_selection;
    back_selection.type = ElementEnum::Edge;

    for (const auto& edge : selections_) {
        back_selection.ids.push_back(edge[0]);
        back_selection.ids.push_back(edge[1]);
    }

    return back_selection;
}

// 用词：picker的picked cell -> selector的selected cell
void EdgeSelectorHighlight::select(double posx, double posy)
{
    vtkNew<vtkHardwarePicker> picker;
    picker->PickFromListOn();
    collection_->InitTraversal();
    for (vtkProp* actor {}; actor = collection_->GetNextProp();) {
        picker->AddPickList(actor);
    }
    picker->Pick(posx, posy, 0, renderer_);

    // 获取选中的CellId （面或者是边）
    vtkIdType pickedCellId = picker->GetCellId();
    if (pickedCellId != -1) {
        // 获取选中的 cell
        vtkActor* pickedActor = picker->GetActor();
        if (!pickedActor)
            return;
        vtkPolyDataMapper* pickedMapper = vtkPolyDataMapper::SafeDownCast(pickedActor->GetMapper());
        if (!pickedMapper) {
            return;
        }
        vtkPolyData* pickedPoly = pickedMapper->GetInput();
        vtkCell* pickedCell = pickedPoly->GetCell(pickedCellId);
        assert(picker && pickedCell);

        // 构建选中边的PolyData
        v_local_id = _find_selected_edge(*picker, *pickedCell);

        // 检查是否已选中
        auto it = std::find_if(selections_.begin(), selections_.end(),
            [&](const std::array<vtkIdType, 2>& id) {
                return _is_selected(v_local_id, std::optional<std::array<vtkIdType, 2>>(id));
            });

        if (it != selections_.end()) {
            // 已选中，取消选中
            selections_.erase(it);
        } else {
            // 未选中，添加
            selections_.push_back(v_local_id);
        }

        // 更新高亮显示
        if (selections_.empty()) {
            _cancel_highlight(selected_mapper_);
            return;
        }

        vtkNew<vtkPoints> points;
        vtkNew<vtkCellArray> lines;

        for (const auto& edge : selections_) {

            std::array<double, 3> position1 {};
            std::array<double, 3> position2 {};
            pickedPoly->GetPoint(edge[0], position1.data());
            pickedPoly->GetPoint(edge[1], position2.data());
            vtkIdType id1 = points->InsertNextPoint(position1.data());
            vtkIdType id2 = points->InsertNextPoint(position2.data());

            vtkNew<vtkLine> line;
            line->GetPointIds()->SetId(0, id1);
            line->GetPointIds()->SetId(1, id2);
            lines->InsertNextCell(line);
        }

        vtkNew<vtkPolyData> highlight_poly;
        highlight_poly->SetPoints(points);
        highlight_poly->SetLines(lines);

        selected_mapper_->SetInputData(highlight_poly);
    } else {
        // 没选到
        clear();
    }
}

void EdgeSelectorHighlight::setCurModelActor(MeshActorSelectOpFactory model_actor)
{
    this->collection_->RemoveAllItems();
    if (auto actor = model_actor.lock()) {
        this->collection_->AddItem(&actor->getEdgeActor());
        this->collection_->AddItem(&actor->getFaceActor());
        this->collection_->AddItem(&actor->getSolidActor());
    }
    this->model_actor_ = model_actor;
}

std::array<vtkIdType, 2> EdgeSelectorHighlight::_find_selected_edge(vtkHardwarePicker& picker, vtkCell& picked_cell)
{
    if (picked_cell.GetCellType() == VTK_LINE)
        return { picked_cell.GetPointId(0), picked_cell.GetPointId(1) };

    double pPos[3] {};
    picker.GetPCoords(pPos);

    vtkNew<vtkIdList> cellIds;
    picked_cell.CellBoundary(0, pPos, cellIds);

    return { cellIds->GetId(0), cellIds->GetId(1) };
}

void EdgeSelectorHighlight::_cancel_highlight(vtkDataSetMapper* selectedMapper)
{
    vtkNew<vtkPolyData> empty;
    selectedMapper->SetInputData(empty);
}

bool EdgeSelectorHighlight::_is_selected(std::array<vtkIdType, 2> v_local_id, const std::optional<std::array<vtkIdType, 2>>& selection)
{
    if (selection) {
        // 选中的边点id，交换意义下对应相同
        const std::array<vtkIdType, 2>& selected1 = *selection;
        const std::array<vtkIdType, 2>& selected2 = v_local_id;
        return selected1[0] == selected2[0] && selected1[1] == selected2[1]
            || selected1[0] == selected2[1] && selected1[1] == selected2[0];
    }
    return false;
}
