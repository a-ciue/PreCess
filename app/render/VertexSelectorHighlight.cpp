#include "CoincidentTopology.h"
#include "MeshActorSelectOp.h"
#include "Selection.h"
#include "SelectorHighlight.h"
#include <optional>
#include <spdlog/spdlog.h>
#include <vtkDataSet.h>
#include <vtkExtractSelection.h>
#include <vtkGeometryFilter.h>
#include <vtkHardwarePicker.h>
#include <vtkMapper.h>
#include <vtkPartitionedDataSet.h>
#include <vtkPointData.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

namespace {
void _cancel_highlight(vtkIdTypeArray* selected_ids)
{
    selected_ids->SetNumberOfValues(0);
    selected_ids->Modified();
}

vtkIdType _is_selected(vtkIdType new_vertex, const vtkIdTypeArray& selected_ids_)
{
    vtkIdTypeArray& selected_ids = const_cast<vtkIdTypeArray&>(selected_ids_);
    vtkIdType id_idx = selected_ids.LookupTypedValue(new_vertex);
    return id_idx;
}
}

VertexSelectorHighlight::VertexSelectorHighlight(vtkRenderer& renderer, vtkPartitionedDataSet& highlight_data,
    unsigned int partition_id, MeshActorSelectOp select_op)
    : renderer_(&renderer)
    , select_op_(std::move(select_op))
    , highlight_data_(&highlight_data)
    , partition_id_(partition_id)
{
    this->selected_ids_->SetNumberOfTuples(1);
    this->selected_ids_->SetNumberOfValues(0);

    extract_filter_ = select_op_.extractVertex(this->selected_ids_);
    geom_filter_->SetInputConnection(extract_filter_->GetOutputPort());
    geom_filter_->Update();
    highlight_data_->SetPartition(partition_id_, geom_filter_->GetOutput());
}

VertexSelectorHighlight::~VertexSelectorHighlight()
{
    highlight_data_->SetPartition(partition_id_, nullptr);
}

SelectionVtk VertexSelectorHighlight::get()
{
    SelectionVtk back_selection;
    back_selection.type = ElementEnum::Vertex;
    for (vtkIdType i = 0; i < selected_ids_->GetNumberOfValues(); ++i) {
        back_selection.ids.push_back(selected_ids_->GetValue(i));
    }
    return back_selection;
}

void VertexSelectorHighlight::clear()
{
    _cancel_highlight(this->selected_ids_);
    geom_filter_->Update();
    highlight_data_->Modified();
}

void VertexSelectorHighlight::clearHighlight()
{
    highlight_data_->SetPartition(partition_id_, nullptr);
    highlight_data_->Modified();
}

void VertexSelectorHighlight::applyHighlight()
{
    geom_filter_->Update();
    highlight_data_->SetPartition(partition_id_, geom_filter_->GetOutput());
    highlight_data_->Modified();
}

void VertexSelectorHighlight::select(double posx, double posy)
{
    // 获取 picked_point_id
    vtkNew<vtkHardwarePicker> picker;
    picker->SnapToMeshPointOn(); // 启用贴近网格点
    picker->SetPixelTolerance(5); // 设置点拾取像素容差
    picker->PickFromListOn();
    picker->AddPickList(&select_op_.getSolidActor());
    picker->AddPickList(&select_op_.getFaceActor());
    picker->AddPickList(&select_op_.getEdgeActor());
    picker->Pick(posx, posy, 0, renderer_);
    vtkIdType picked_point_id = picker->GetPointId();
    if (picked_point_id == -1) {
        spdlog::debug("VertexSelectorHighlight::select: no point picked.");
        return;
    }
    vtkDataSet* picked_data_set = picker->GetDataSet();

    // 获取对应的点id selected_vertex_id
    auto vertex_id_array = vtkIdTypeArray::SafeDownCast(picked_data_set->GetPointData()->GetArray("vtkOriginalPointIds"));
    if (!vertex_id_array) {
        clear();
        spdlog::debug("Picked cell id: {}, no vertex id array found.", picked_point_id);
        return;
    }
    vtkIdType selected_vertex_id = vertex_id_array->GetValue(picked_point_id);

    // 检查该点是否已经被选中
    vtkIdType id_idx = _is_selected(selected_vertex_id, *this->selected_ids_);
    if (id_idx >= 0) {
        // 已选中，取消选中
        selected_ids_->RemoveTuple(id_idx);
        spdlog::debug("VertexSelectorHighlight::select: point {} deselected.", selected_vertex_id);
    } else {
        // 未选中，添加选中
        selected_ids_->InsertNextValue(selected_vertex_id);
        selected_ids_->ClearLookup(); // 清除查找缓存，确保下一次查找正确
        spdlog::debug("VertexSelectorHighlight::select: point {} selected.", selected_vertex_id);
    }

    selected_ids_->Modified();
    applyHighlight();
}

void VertexSelectorHighlight::setupHighlightStyle(vtkActor& actor, vtkMapper& mapper)
{
    mapper.SetRelativeCoincidentTopologyPointOffsetParameter(highlight::POINT_UNITS);

    actor.SetMapper(&mapper);
    vtkNew<vtkProperty> prop;
    prop->SetColor(1.0, 0.0, 0.0);
    prop->SetPointSize(6.0);
    actor.SetProperty(prop);
}
