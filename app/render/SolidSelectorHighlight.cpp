#include "CoincidentTopology.h"
#include "MeshActorSelectOp.h"
#include "SelectorHighlight.h"

#include <vtkActor.h>
#include <vtkCellData.h>
#include <vtkDataSet.h>
#include <vtkExtractSelection.h>
#include <vtkGeometryFilter.h>
#include <vtkHardwarePicker.h>
#include <vtkIdTypeArray.h>
#include <vtkMapper.h>
#include <vtkPartitionedDataSet.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

#include <spdlog/spdlog.h>

namespace {
void _cancel_highlight(vtkIdTypeArray* selected_ids)
{
    selected_ids->SetNumberOfValues(0);
    selected_ids->Modified();
}

vtkIdType _is_selected(vtkIdType new_solid, const vtkIdTypeArray& selected_ids_)
{
    vtkIdTypeArray& selected_ids = const_cast<vtkIdTypeArray&>(selected_ids_);
    vtkIdType id_idx = selected_ids.LookupTypedValue(new_solid);
    return id_idx;
}
}

SolidSelectorHighlight::SolidSelectorHighlight(vtkRenderer& renderer, vtkPartitionedDataSet& highlight_data,
    unsigned int partition_id, MeshActorSelectOp select_op)
    : renderer_(&renderer)
    , select_op_(std::move(select_op))
    , highlight_data_(&highlight_data)
    , partition_id_(partition_id)
{
    this->selected_ids_->SetName("vtkOriginalCellIds");
    this->selected_ids_->SetNumberOfComponents(1);
    this->selected_ids_->SetNumberOfValues(0);

    extract_filter_ = select_op_.extractSolid(selected_ids_);
    geom_filter_->SetInputConnection(extract_filter_->GetOutputPort());
    geom_filter_->Update();
    highlight_data_->SetPartition(partition_id_, geom_filter_->GetOutput());
}

SolidSelectorHighlight::~SolidSelectorHighlight()
{
    highlight_data_->SetPartition(partition_id_, nullptr);
}

SelectionVtk SolidSelectorHighlight::get()
{
    SelectionVtk back_selection;
    back_selection.type = ElementEnum::Solid;
    for (vtkIdType i = 0; i < selected_ids_->GetNumberOfValues(); ++i) {
        back_selection.ids.push_back(selected_ids_->GetValue(i));
    }
    return back_selection;
}

void SolidSelectorHighlight::clear()
{
    _cancel_highlight(this->selected_ids_);
    disableHighlight();
}

void SolidSelectorHighlight::disableHighlight()
{
    highlight_data_->SetPartition(partition_id_, nullptr);
    highlight_data_->Modified();
}

void SolidSelectorHighlight::enableHighlight()
{
    if (selected_ids_->GetNumberOfValues() == 0) {
        disableHighlight();
        return;
    }
    geom_filter_->Update();
    highlight_data_->SetPartition(partition_id_, geom_filter_->GetOutput());
    highlight_data_->Modified();
}

void SolidSelectorHighlight::select(double posx, double posy)
{
    // 获取 picked_cell_id和picked_data_set
    vtkNew<vtkHardwarePicker> picker;
    picker->PickFromListOn();
    picker->AddPickList(&select_op_.getSolidActor());
    picker->Pick(posx, posy, 0, renderer_);
    vtkIdType picked_cell_id = picker->GetCellId();
    if (picked_cell_id < 0) {
        clear();
        spdlog::debug("No cell picked, selection cleared.");
        return;
    }
    vtkDataSet* picked_data_set = picker->GetDataSet();

    // 获取对应的体id selected_solid_id
    auto solid_id_array = vtkIdTypeArray::SafeDownCast(picked_data_set->GetCellData()->GetArray("vtkOriginalCellIds"));
    if (!solid_id_array) {
        clear();
        spdlog::debug("Picked cell id: {}, no solid id array found.", picked_cell_id);
        return;
    }
    vtkIdType selected_solid_id = solid_id_array->GetValue(picked_cell_id);

    // 检查该体是否已经被选中
    vtkIdType id_idx = _is_selected(selected_solid_id, *this->selected_ids_);
    if (id_idx >= 0) {
        // 取消选中
        selected_ids_->RemoveTuple(id_idx);
        spdlog::debug("SolidSelectorHighlight::select: point {} canceled.", selected_solid_id);
    } else {
        // 未选中，添加选中
        selected_ids_->InsertNextValue(selected_solid_id);
        selected_ids_->ClearLookup(); // 清除查找缓存，确保下一次查找正确
        spdlog::debug("SolidSelectorHighlight::select: point {} selected.", selected_solid_id);
    }
    this->selected_ids_->Modified();
    enableHighlight();
}

void SolidSelectorHighlight::setupHighlightStyle(vtkActor& actor, vtkMapper& mapper)
{
    mapper.SetRelativeCoincidentTopologyPolygonOffsetParameters(0, highlight::SOLID_UNITS);

    actor.SetMapper(&mapper);
    vtkNew<vtkProperty> prop;
    prop->SetColor(1.0, 1.0, 0.0); // 黄色高亮
    prop->EdgeVisibilityOn();
    prop->SetEdgeColor(1.0, 0.0, 0.0); // 红色边框
    prop->SetLineWidth(2.0);
    actor.SetProperty(prop);
}
