#include "MeshActorSelectOp.h"
#include "Selection.h"
#include "SelectorHighlight.h"
#include <spdlog/spdlog.h>
#include <vtkHardwarePicker.h>
#include <vtkPartitionedDataSet.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

namespace {
std::vector<vtkIdType>::const_iterator _find_selected(vtkIdType new_face_id, const std::vector<vtkIdType>& selections)
{
    return std::find_if(selections.begin(), selections.end(),
        [&](const vtkIdType& id) {
            return id == new_face_id;
        });
}
}

FaceSelectorHighlight::FaceSelectorHighlight(vtkRenderer& renderer, vtkPartitionedDataSet& highlight_data,
    unsigned int partition_id, MeshActorSelectOp select_op)
    : renderer_(&renderer)
    , select_op_(std::move(select_op))
    , highlight_data_(&highlight_data)
    , partition_id_(partition_id)
{
    selections_poly_ = vtkSmartPointer<vtkPolyData>::New();
    highlight_data_->SetPartition(partition_id_, selections_poly_);
}

FaceSelectorHighlight::~FaceSelectorHighlight()
{
    highlight_data_->SetPartition(partition_id_, nullptr);
}

SelectionVtk FaceSelectorHighlight::get()
{
    SelectionVtk back_selection;
    back_selection.type = ElementEnum::Face;

    for (const auto& face : selections_) {
        back_selection.ids.push_back(face);
    }

    return back_selection;
}

void FaceSelectorHighlight::clear()
{
    selections_.clear();
    selections_poly_->Initialize();
    highlight_data_->Modified();
}

void FaceSelectorHighlight::select(double posx, double posy)
{
    vtkNew<vtkHardwarePicker> picker;
    picker->PickFromListOn();
    picker->AddPickList(&select_op_.getFaceActor());
    picker->Pick(posx, posy, 0, renderer_);

    vtkIdType picked_cell_id = picker->GetCellId();
    if (picked_cell_id == -1) {
        clear();
        return;
    }

    // 获取选中的 cell
    vtkActor* picked_actor = picker->GetActor();
    assert(picked_actor);
    vtkPolyDataMapper* picked_mapper = vtkPolyDataMapper::SafeDownCast(picked_actor->GetMapper());
    assert(picked_mapper);
    vtkPolyData* picked_poly = picked_mapper->GetInput();
    assert(picked_poly);

    // 检查是否已选中
    auto it = _find_selected(picked_cell_id, selections_);
    if (it != selections_.end()) { // 已选中，取消选中
        selections_.erase(it);
    } else { // 未选中，添加
        selections_.push_back(picked_cell_id);
    }

    vtkNew<vtkCellArray> cell_array;
    for (const auto& face : selections_) {
        cell_array->InsertNextCell(picked_poly->GetCell(face));
    }

    vtkNew<vtkPolyData> highlight_poly;
    highlight_poly->SetPoints(picked_poly->GetPoints()); // 使用原始数据的点集
    highlight_poly->SetPolys(cell_array); // 设置面单元
    selections_poly_->ShallowCopy(highlight_poly);
    highlight_data_->Modified();
}

void FaceSelectorHighlight::setupHighlightStyle(vtkActor& actor, vtkMapper& mapper)
{
    mapper.SetRelativeCoincidentTopologyPolygonOffsetParameters(0, -2);

    actor.SetMapper(&mapper);
    vtkNew<vtkProperty> prop;
    prop->SetColor(1.0, 0.0, 0.0); // 红色高亮
    prop->SetLineWidth(2.0);
    prop->EdgeVisibilityOn();
    prop->SetEdgeColor(1.0, 0.0, 0.0);
    actor.SetProperty(prop);
}
