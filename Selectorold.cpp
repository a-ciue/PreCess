//#include "Selector.h"
//#include "Selector.h"
#include <array>
#include <optional>
#include <utility>
#include <vector>
#include <vtkActor.h>
#include <vtkCellData.h>
#include <vtkCellPicker.h>
#include <vtkMapper.h>
#include <vtkNew.h>
#include <vtkNamedColors.h>
#include <vtkDataSetMapper.h>
#include <vtkPropPicker.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkPolyDataMapper.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkPolyDataMapper.h>
#include <vtkSelectionNode.h>
#include <vtkUnstructuredGrid.h>
#include <vtkExtractSelection.h>
#include <vtkLine.h>
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkPolyDataMapper.h>
#include <vtkPropAssembly.h>
#include <vtkAssembly.h>
#include <vtkAssemblyPath.h>
#include <vtkAssemblyNode.h>
#include <vtkAppendPolyData.h>
#include <vtkMapper.h>
#include "ModelUtil.h"
#include <vtkCompositeDataDisplayAttributes.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkCompositePolyDataMapper.h>

//namespace Selector {
//    std::optional<std::pair<vtkActor*, int>> pick_cell(double posx, double posy, vtkRenderer* renderer) {
//        //vtkNew<vtkPropPicker> actorpicker;
//        vtkNew<vtkCellPicker> cellpicker;
//        cellpicker->Pick(posx, posy, 0, renderer);
//        
//        vtkNew<vtkCellPicker> picker;
//        picker->Pick(posx, posy, 0, renderer);
//        vtkProp* pickedprop = picker->GetPropAssembly();
//        vtkActor* actor{};
//
//        if (picker->GetCellId() != -1) {
//            vtkPropAssembly* picked_assembly = picker->GetPropAssembly();
//            vtkAssemblyPath* path = picker->GetPath();          
//            actor = vtkActor::SafeDownCast(path->GetLastNode()->GetViewProp());
//        }
//
//        if (!actor) {
//
//            return std::nullopt;
//        }
//
//        vtkIdType cellId = cellpicker->GetCellId();
//
//        // 如果cellId为-1，表示没有拾取到单元格
//        if (cellId == -1) {
//            return std::nullopt;
//        }
//
//        // 在Actor中的位置
//        int localCellIndex = cellpicker->GetCellId();
//
//
//        // 返回actor和单元格的索引
//        return std::make_pair(actor, localCellIndex);
//    }
//}


void BlockSelectorHighlight::clear() {
    // 取消高亮所有选中的actor
    for (auto& selection : selections_) {
        _cancel_highlight(selection);
    }
    selections_.clear();
}

std::vector<vtkActor*> BlockSelectorHighlight::get() {
    // 返回当前选中的actors
    std::vector<vtkActor*> actors;
    for (const auto& selection : selections_) {
        actors.push_back(selection.actor);
    }
    return actors;
}
vtkPropAssembly* BlockSelectorHighlight::getAssembly()
{
    vtkPropAssembly* backassembly = actorassembly;
    return backassembly;

}

void BlockSelectorHighlight::select(double posx, double posy) {
    // 找到actor，高亮
    vtkNew<vtkCellPicker> picker;
    //vtkNew<vtkCellPicker> picker2;
    picker->Pick(posx, posy, 0, renderer_);
    //picker2->Pick(posx, posy, 0, renderer_);
    if (picker->GetPropAssembly() != nullptr) {
        vtkProp* pickedprop = picker->GetPropAssembly();
        actorassembly = picker->GetPropAssembly();
        vtkAssemblyPath* path = picker->GetPath();
        double *a=picker->GetPickPosition();
        cout << a[0]<<"   "<< a[1] << "   " << a[2] << endl;
        vtkActor* new_actor = vtkActor::SafeDownCast(path->GetLastNode()->GetViewProp());
        std::optional<size_t> selected_index = _is_selected(new_actor, selections_);
        if (selected_index) {
            _cancel_highlight(selections_[*selected_index]);
            selections_.erase(selections_.begin() + *selected_index);
        } else {
            //
            selections_.emplace_back(Actor { new_actor, vtkSmartPointer<vtkProperty>::New() });
            selections_.back().backup_property->DeepCopy(new_actor->GetProperty());
            new_actor->GetProperty()->SetColor(1.0, 0.0, 0.0);
            renderer_->AddActor(new_actor);

        }
    }
}

void BlockSelectorHighlight::_cancel_highlight(Actor& selection) {
    // 取消高亮，修改回原来属性
    //selection.actor->SetProperty(selection.backup_property);
    selection.actor->GetProperty()->SetColor(selection.backup_property->GetColor());
}

std::optional<size_t> BlockSelectorHighlight::_is_selected(const vtkActor* new_actor, const std::vector<Actor>& selections) {
    
    for (size_t i = 0; i < selections.size(); ++i) {
        if (selections[i].actor == new_actor) {
            // 如果找到,返回该 actor 的索引
            return i;
        }
    }
    return std::nullopt;
}

SingleFaceSelectorHighlight::SingleFaceSelectorHighlight(vtkMapper* selection_mapper)
    : renderer_(renderer) {
    selectedActor_ = vtkSmartPointer<vtkActor>::New();
    renderer_->AddActor(selectedActor_);
}

SingleFaceSelectorHighlight::~SingleFaceSelectorHighlight() {
    
    renderer_->RemoveActor(selectedActor_);
}

std::optional<SingleFaceSelectorHighlight::SelectedFace> SingleFaceSelectorHighlight::get() {
    
    return selection_;
}

vtkPropAssembly* SingleFaceSelectorHighlight::getAssembly()
{
    vtkPropAssembly* backassembly = faceassembly;
    return backassembly;

}

void SingleFaceSelectorHighlight::clear() {
    // 清空selection取消高亮
    _cancel_highlight(selectedActor_, renderer_);
    selection_ = std::nullopt;
}

void SingleFaceSelectorHighlight::select(double posx, double posy) {
    // 找到face并存储
    auto result = Selector::pick_cell(posx, posy, renderer_);
    vtkNew<vtkCellPicker> picker;
    picker->Pick(posx, posy, 0, renderer_);

    if (picker->GetCellId() != -1) {
        vtkProp* pickedprop = picker->GetPropAssembly();
        faceassembly = picker->GetPropAssembly();
    }

    if (result) {
        vtkActor* new_actor = result->first;
        int local_id = result->second;
        SelectedFace new_face = { new_actor, local_id };

        if (_is_selected(new_face, selection_, selectedActor_)) {
           
            _cancel_highlight(selectedActor_, renderer_);
            selection_ = std::nullopt;
        }
        else {
            // 选中新的face
            selection_ = new_face;

            vtkNew<vtkIdTypeArray> ids;
            ids->SetNumberOfComponents(1);
            ids->InsertNextValue(new_face.local_id);

            vtkNew<vtkSelectionNode> selectionNode;
            selectionNode->SetFieldType(vtkSelectionNode::CELL);
            selectionNode->SetContentType(vtkSelectionNode::INDICES);
            selectionNode->SetSelectionList(ids);

            vtkNew<vtkSelection> selection;
            selection->AddNode(selectionNode);

            vtkPolyDataMapper* patch_polydata = vtkPolyDataMapper::SafeDownCast(new_face.patch_actor->GetMapper()); 
            vtkNew<vtkExtractSelection> extractSelection;
            extractSelection->SetInputData(0, patch_polydata->GetInput());
            extractSelection->SetInputData(1, selection);
            extractSelection->Update();

            // In selection
            vtkNew<vtkUnstructuredGrid> selected;
            selected->ShallowCopy(extractSelection->GetOutput());            

            vtkNew<vtkDataSetMapper> selectedMapper;
            selectedMapper->SetInputData(selected);
            selectedActor_->SetMapper(selectedMapper);
            selectedActor_->GetProperty()->EdgeVisibilityOn();
            selectedActor_->GetProperty()->SetColor(1.0, 0.1, 0.1);
        }
    }
    else {
        _cancel_highlight(selectedActor_, renderer_);
        selection_ = std::nullopt;
    }
}

void SingleFaceSelectorHighlight::_cancel_highlight(vtkSmartPointer<vtkActor>& selectedActor, vtkRenderer* renderer) {
    // 取消高亮,清空mapper
    renderer->RemoveActor(selectedActor);
    selectedActor = vtkSmartPointer<vtkActor>::New();
    renderer->AddActor(selectedActor);
}

bool SingleFaceSelectorHighlight::_is_selected(SelectedFace new_face, const std::optional<SelectedFace>& selection, vtkActor* selectedActor) {
    // 选中网格或选中选择actor
    // if 选中选择actor
    //   return true
    // else 选中网格
    //   return 选中单元一致
   
    if (new_face.patch_actor == selectedActor || selection && selection->patch_actor == new_face.patch_actor && selection->local_id == new_face.local_id) {
        return true;
    }
    return false;
}

SingleEdgeSelectorHighlight::SingleEdgeSelectorHighlight(vtkMapper* selection_mapper)
{
    selectedActor_ = vtkSmartPointer<vtkActor>::New();
    renderer_->AddActor(selectedActor_);
}

SingleEdgeSelectorHighlight::~SingleEdgeSelectorHighlight()
{
    renderer_->RemoveActor(selectedActor_);
}

std::optional<SingleEdgeSelectorHighlight::SelectedEdge> SingleEdgeSelectorHighlight::get()
{
    return selection_;
}

vtkPropAssembly* SingleEdgeSelectorHighlight::getAssembly()
{
    vtkPropAssembly* backassembly = edgeassembly;
    return backassembly;

}

void SingleEdgeSelectorHighlight::clear()
{
    _cancel_highlight(selectedMapper_ ,selectedActor_);
    selection_ = std::nullopt;
}

void SingleEdgeSelectorHighlight::select(double posx, double posy)
{
    vtkNew<vtkCellPicker> picker;
    picker->Pick(posx, posy, 0, renderer_);
    vtkProp* pickedprop = picker->GetPropAssembly();

    if (picker->GetCellId() != -1) {
        edgeassembly = picker->GetPropAssembly();
        vtkAssemblyPath* path = picker->GetPath();
        std::cout << "some Prop: " << vtkActor::SafeDownCast(path->GetLastNode()->GetViewProp()) << std::endl;
        vtkActor* picked_actor= vtkActor::SafeDownCast(path->GetLastNode()->GetViewProp());
        double pPos[3] {};
        picker->GetPCoords(pPos);
        pPos[2] = 1 - pPos[1] - pPos[0];

        std::array<double, 3> position1 {};
        std::array<double, 3> position2 {};
        vtkNew<vtkLine> line0;
        vtkNew<vtkPoints> points;
        vtkNew<vtkCellArray> lines;
        vtkSmartPointer<vtkPolyData> data;
        vtkPolyDataMapper* mapper = vtkPolyDataMapper::SafeDownCast(picked_actor->GetMapper());
        data = mapper->GetInput();
        vtkIdType* cellpid = data->GetCell(picker->GetCellId())->GetPointIds()->GetPointer(0);

        SelectedEdge picked_edge;
        picked_edge.actor = picked_actor;
        /**/ if (pPos[1] < pPos[0] && pPos[1] < pPos[2]) {
            picked_edge.v_local_id[0] = cellpid[0];
            picked_edge.v_local_id[1] = cellpid[1];
        } else if (pPos[2] < pPos[0] && pPos[2] < pPos[1]) {
            picked_edge.v_local_id[0] = cellpid[1];
            picked_edge.v_local_id[1] = cellpid[2];
        } else if (pPos[0] < pPos[1] && pPos[0] < pPos[2]) {
            picked_edge.v_local_id[0] = cellpid[0];
            picked_edge.v_local_id[1] = cellpid[2];
        }
        data->GetPoint(picked_edge.v_local_id[0], position1.data());
        data->GetPoint(picked_edge.v_local_id[1], position2.data());
        points->InsertNextPoint(position1.data());
        points->InsertNextPoint(position2.data());
        
        line0->GetPointIds()->SetId(0, 0);
        line0->GetPointIds()->SetId(1, 1);
        lines->InsertNextCell(line0);

        vtkNew<vtkPolyData> polydata;
        polydata->SetPoints(points);
        polydata->SetLines(lines);
        selectedMapper_->SetInputData(polydata);
        selectedMapper_->Update(); // 更新映射器

        // 创建一个演员来显示这些线段

        selectedActor_->SetMapper(selectedMapper_);
        selectedActor_->GetProperty()->SetColor(ModelUtil::colors->GetColor3d("black").GetData());
        selectedActor_->GetProperty()->SetLineWidth(5); // 设置

        if (_is_selected(picked_edge, selection_, selectedActor_)) {
            _cancel_highlight(selectedMapper_, selectedActor_);
            selection_ = std::nullopt;
        }
        else
        {
            selection_ = picked_edge;
        }
    }

    else if (picker->GetCellId() == -1) {
        // 没选到
        _cancel_highlight(selectedMapper_, selectedActor_);
        selection_ = std::nullopt;
    }
}

void SingleEdgeSelectorHighlight::_cancel_highlight(vtkDataSetMapper* selectedMapper, vtkActor* selectedActor)
{
    vtkNew<vtkPolyData> empty;
    selectedMapper->SetInputData(empty);
    selectedActor->SetMapper(selectedMapper);
}

bool SingleEdgeSelectorHighlight::_is_selected(SelectedEdge new_edge, const std::optional<SelectedEdge>& selection, vtkActor* selectedActor)
{
    if (new_edge.actor == selectedActor)
        // 选中了selectedActor
        return true;
    if (selection && selection->actor == new_edge.actor) {
        // 选中的边点id，交换意义下对应相同
        const std::array<int, 2>& selected1 = selection->v_local_id;
        const std::array<int, 2>& selected2 = new_edge.v_local_id;
        return selected1[0] == selected2[0] && selected1[1] == selected2[1]
            || selected1[0] == selected2[1] && selected1[1] == selected2[0];
    }
    return false;
}
