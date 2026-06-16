#include "MeshActorSelectOp.h"
#include "Selection.h"
#include "SelectorHighlight.h"
#include <optional>
#include <spdlog/spdlog.h>
#include <vtkDataSetMapper.h>
#include <vtkHardwarePicker.h>
#include <vtkPointData.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkUnstructuredGrid.h>

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

VertexSelectorHighlight::VertexSelectorHighlight(vtkRenderer* renderer)
    : renderer_(renderer)
{
    vtkNew<vtkUnstructuredGrid> dummy;
    vtkNew<vtkDataSetMapper> mapper;
    mapper->SetInputData(dummy);

    this->highlight_actor_->SetMapper(mapper);
    this->highlight_actor_->GetProperty()->SetColor(1.0, 0.0, 0.0); // 红色高亮
    this->highlight_actor_->GetProperty()->SetPointSize(6.0);
    this->highlight_actor_->PickableOff(); // 防止自己被选中

    this->selected_ids_->SetNumberOfTuples(1);
    this->selected_ids_->SetNumberOfValues(0);

    this->renderer_->AddActor(this->highlight_actor_);
}

VertexSelectorHighlight::~VertexSelectorHighlight()
{
    clear();
    this->renderer_->RemoveActor(this->highlight_actor_);
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
}

void VertexSelectorHighlight::select(double posx, double posy)
{
    // 获取 picked_point_id
    vtkNew<vtkHardwarePicker> picker;
    picker->SnapToMeshPointOn(); // 启用贴近网格点
    picker->SetPixelTolerance(5); // 设置点拾取像素容差
    picker->PickFromListOn();
    auto model_actor = model_actor_.lock();
    if (!model_actor) {
        clear();
        spdlog::debug("VertexSelectorHighlight::select: model_actor is null, selection cleared.");
        return;
    }
    picker->AddPickList(&model_actor->getSolidActor());
    picker->AddPickList(&model_actor->getFaceActor());
    picker->AddPickList(&model_actor->getEdgeActor());
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
    selected_ids_->Modified(); // 通知 VTK 数据已更改，进行刷新
}

void VertexSelectorHighlight::setCurModelActor(MeshActorSelectOpFactory model_actor)
{
    this->model_actor_ = model_actor;
    if (auto model_actor = this->model_actor_.lock()) {
        auto extract_selection = model_actor->extractVertex(this->selected_ids_);
        this->highlight_actor_->GetMapper()->SetInputConnection(extract_selection->GetOutputPort());
    }
}
