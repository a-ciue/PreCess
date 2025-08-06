#include "Selector.h"
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
#include <vtkUnstructuredGrid.h>
#include <vtkExtractSelection.h>
#include <vtkLine.h>
#include <vtkPropAssembly.h>
#include <vtkAssemblyPath.h>
#include <vtkAppendPolyData.h>
#include <vtkCompositeDataDisplayAttributes.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkCompositePolyDataMapper.h>
#include "MeshActor.h"
#include "Selection.h"

BlockSelectorHighlight::BlockSelectorHighlight(vtkRenderer* renderer)
{
	this->renderer_= renderer;
}

void BlockSelectorHighlight::clear() {
    // 取消高亮所有选中的actor
    for (auto& selection : selections_) {
        _cancel_highlight(selection);
    }
    selections_.clear();
}

SelectionVtk BlockSelectorHighlight::get() {
    SelectionVtk back_selection;
    back_selection.type = ElementEnum::Block;
    for (const auto& selection : selections_) {
        back_selection.ids.push_back(selection.block_id);  
    }
    return back_selection;
}

void BlockSelectorHighlight::select(double posx, double posy) {
    vtkNew<vtkCellPicker> picker;
    picker->PickFromListOn();
    collection_->InitTraversal();
    for (vtkProp* actor{}; actor = collection_->GetNextProp();) {
        picker->AddPickList(actor);
    }

    picker->Pick(posx, posy, 0, this->renderer_);
    vtkProp* pickedProp = picker->GetViewProp();

    if (!pickedProp || !pickedProp->IsA("vtkActor")) {
        std::cout << "No valid object picked!" << std::endl;
        for (auto& selection : selections_) {
            _cancel_highlight(selection);
        }
        selections_.clear();  // 清空选择列表

        return;
    }

    vtkActor* actor = vtkActor::SafeDownCast(pickedProp);
    vtkMapper* getMapper = actor->GetMapper();
    if (!getMapper->IsA("vtkCompositePolyDataMapper")) {
        std::cout << "Not a composite mapper!" << std::endl;
        return;
    }

    this->mapper_ = vtkCompositePolyDataMapper::SafeDownCast(getMapper);
    unsigned int blockIndex = picker->GetFlatBlockIndex();
    blockIndex--;
    std::optional<size_t> selected_index = _is_selected(blockIndex, selections_);
    if (selected_index) {
        // ✅ 取消高亮，并恢复原颜色
        _cancel_highlight(selections_[*selected_index]);
        selections_.erase(selections_.begin() + *selected_index);
    }
    else {
        // ✅ 新增高亮
        selections_.emplace_back();
        Block& sel = selections_.back();
        sel.block_id = blockIndex;
        auto multiblock = vtkMultiBlockDataSet::SafeDownCast(this->mapper_->GetInputDataObject(0, 0));
        if (!multiblock || blockIndex >= multiblock->GetNumberOfBlocks()) return;

        auto block = vtkPolyData::SafeDownCast(multiblock->GetBlock(blockIndex));
        if (!block) return;

        auto colors = vtkUnsignedCharArray::SafeDownCast(block->GetCellData()->GetScalars());
        if (colors && colors->GetNumberOfTuples() > 0) {
            // ✅ 保存 block 第一个 cell 的原始颜色
            sel.backup_color[0] = colors->GetComponent(0, 0) / 255.0;
            sel.backup_color[1] = colors->GetComponent(0, 1) / 255.0;
            sel.backup_color[2] = colors->GetComponent(0, 2) / 255.0;
        }
        else {
            // 默认白色
            sel.backup_color[0] = 1.0;
            sel.backup_color[1] = 1.0;
            sel.backup_color[2] = 1.0;
        }

        highlightBlockByCellColor(this->mapper_, blockIndex, 255, 0, 0);  // 高亮为红色
    }

    // 刷新 actor 显示
    actor->SetMapper(this->mapper_);
    this->renderer_->RemoveActor(actor);  // 可选刷新方法
    this->renderer_->AddActor(actor);
}



void BlockSelectorHighlight::highlightBlockByCellColor(vtkCompositePolyDataMapper* mapper, unsigned int block_index,
    unsigned char r, unsigned char g, unsigned char b)
{
    auto multiblock = vtkMultiBlockDataSet::SafeDownCast(mapper->GetInputDataObject(0, 0));
    if (!multiblock || block_index >= multiblock->GetNumberOfBlocks()) return;

    auto block = vtkPolyData::SafeDownCast(multiblock->GetBlock(block_index));
    if (!block) return;

    auto colors = vtkUnsignedCharArray::SafeDownCast(block->GetCellData()->GetScalars());
    if (!colors || colors->GetNumberOfComponents() != 3) return;

    for (vtkIdType i = 0; i < colors->GetNumberOfTuples(); ++i) {
        colors->SetTypedTuple(i, std::array<unsigned char, 3>{r, g, b}.data());
    }

    block->GetCellData()->SetScalars(colors);
    block->Modified();  // 通知 VTK 数据已更新
}
vtkPropCollection* BlockSelectorHighlight::getPickList()
{
    return this->collection_;
}

void BlockSelectorHighlight::_cancel_highlight(Block& selection) {
    // 获取 multiblock 数据集
    auto multiblock = vtkMultiBlockDataSet::SafeDownCast(this->mapper_->GetInputDataObject(0, 0));
    if (!multiblock || selection.block_id >= multiblock->GetNumberOfBlocks()) {
        std::cout << "Invalid block index!" << std::endl;
        return;
    }

    // 获取 block 和颜色数据
    auto block = vtkPolyData::SafeDownCast(multiblock->GetBlock(selection.block_id));
    if (!block) {
        std::cout << "Invalid block!" << std::endl;
        return;
    }

    auto colors = vtkUnsignedCharArray::SafeDownCast(block->GetCellData()->GetScalars());
    if (!colors || colors->GetNumberOfComponents() != 3) {
        std::cout << "No valid color data!" << std::endl;
        return;
    }

    // 恢复颜色
    unsigned char r = static_cast<unsigned char>(selection.backup_color[0] * 255);
    unsigned char g = static_cast<unsigned char>(selection.backup_color[1] * 255);
    unsigned char b = static_cast<unsigned char>(selection.backup_color[2] * 255);

    // 遍历每个 cell 恢复颜色
    for (vtkIdType i = 0; i < colors->GetNumberOfTuples(); ++i) {
        colors->SetTypedTuple(i, std::array<unsigned char, 3>{r, g, b}.data());
    }

    // 更新 block 数据
    block->GetCellData()->SetScalars(colors);
    block->Modified();  // 通知 VTK 数据已更改，进行刷新
}


std::optional<size_t> BlockSelectorHighlight::_is_selected(const vtkIdType block_id, const std::vector<Block>& selections) {

    for (size_t i = 0; i < selections.size(); ++i) {
        if (selections[i].block_id == block_id) {
            // 如果找到,返回该 actor 的索引
            return i;
        }
    }
    return std::nullopt;
}

SingleFaceSelectorHighlight::SingleFaceSelectorHighlight(vtkRenderer* renderer) {
    this->renderer_ = renderer;
}

SingleFaceSelectorHighlight::~SingleFaceSelectorHighlight()
{
    _cancel_highlight(selection_, renderer_);
    selection_ = std::nullopt;
}

SelectionVtk SingleFaceSelectorHighlight::get()
{
    SelectionVtk back_selection;
    back_selection.type = ElementEnum::Face;
    if (selection_.has_value()) {
        back_selection.ids.push_back(selection_.value());
    }
    return back_selection;
}

void SingleFaceSelectorHighlight::clear()
{
    _cancel_highlight(selection_, renderer_);
    selection_ = std::nullopt;
}

void SingleFaceSelectorHighlight::select(double posx, double posy)
{
    vtkSmartPointer<vtkCellPicker> picker = vtkSmartPointer<vtkCellPicker>::New();
    picker->PickFromListOn();
    collection_->InitTraversal();
    for (vtkProp* actor{}; actor = collection_->GetNextProp();  )
    {
        picker->AddPickList(actor);
    }
    picker->Pick(posx, posy, 0, this->renderer_);

    vtkIdType cellId = picker->GetCellId();
    if (cellId < 0) {
        std::cout << "No triangle was picked." << std::endl;
        return ;
    }
    if (_is_selected(cellId, selection_)) {
        // 再次点击取消选中
        clear();
        return;
    }

    // 更新选中
    selection_ = cellId;

    // 创建一个只包含该面的 polydata 用于高亮
    vtkSmartPointer<vtkPolyData> input = vtkPolyData::SafeDownCast(picker->GetDataSet());
    if (!input) return;

    vtkSmartPointer<vtkCellArray> cell_array = vtkSmartPointer<vtkCellArray>::New();
    cell_array->InsertNextCell(input->GetCell(cellId));

    vtkSmartPointer<vtkPolyData> single_face = vtkSmartPointer<vtkPolyData>::New();
    single_face->SetPoints(input->GetPoints());
    single_face->SetPolys(cell_array);

    this->mapper_->SetInputDataObject(single_face);

    set_highlight_actor();
    // 添加到渲染器中
    renderer_->AddActor(highlight_actor_);
    //renderer_->Render();
}

vtkPropCollection* SingleFaceSelectorHighlight::getPickList()
{
    return this->collection_;
}

void SingleFaceSelectorHighlight::_cancel_highlight(std::optional<vtkIdType> selection, vtkRenderer* renderer)
{
    this->renderer_->RemoveActor(this->highlight_actor_);
    //renderer->Render();
}

bool SingleFaceSelectorHighlight::_is_selected(vtkIdType new_face_id, const std::optional<vtkIdType>& selection)
{
    return selection.has_value() && selection.value() == new_face_id;
}

void SingleFaceSelectorHighlight::set_highlight_actor()
{
    mapper_->ScalarVisibilityOff();
    
    this->highlight_actor_->SetMapper(mapper_);
    this->highlight_actor_->GetProperty()->SetColor(1.0, 0.0, 0.0);  // 红色高亮
    this->highlight_actor_->GetProperty()->SetLineWidth(2.0);
    this->highlight_actor_->GetProperty()->EdgeVisibilityOn();
    this->highlight_actor_->GetProperty()->SetEdgeColor(1.0, 0.0, 0.0);
    this->highlight_actor_->PickableOff();  // 防止自己被选中
    
}

SingleEdgeSelectorHighlight::SingleEdgeSelectorHighlight(vtkRenderer* renderer) {
    this->selected_actor_ = vtkSmartPointer<vtkActor>::New();
    this->renderer_ = renderer;
}

SingleEdgeSelectorHighlight::~SingleEdgeSelectorHighlight()
{
    renderer_->RemoveActor(selected_actor_);
}

void SingleEdgeSelectorHighlight::clear()
{
    _cancel_highlight(selected_mapper_, selected_actor_);
    selection_ = std::nullopt;
}

SelectionVtk SingleEdgeSelectorHighlight::get()
{
    SelectionVtk back_selection;
    back_selection.type = ElementEnum::Edge;

    if (selection_.has_value()) {
        back_selection.ids.push_back(this->selection_->v_local_id[0]);
        back_selection.ids.push_back(this->selection_->v_local_id[1]);
    }

    return back_selection;
}

void SingleEdgeSelectorHighlight::select(double posx, double posy)
{
    vtkNew<vtkCellPicker> picker;
    picker->PickFromListOn();
    collection_->InitTraversal();
    for (vtkProp* actor{}; actor = collection_->GetNextProp(); )
    {
        picker->AddPickList(actor);
    }
    picker->Pick(posx, posy, 0, renderer_);

	// 获取选中的三角形的 CellId
	vtkIdType pickedCellId = picker->GetCellId();
    if (picker->GetCellId() != -1) {
        vtkActor* pickedActor = picker->GetActor();
        if (!pickedActor) return;

        // 获取 mapper 的输入数据（假设是 vtkPolyData）
        vtkPolyData* polyData = vtkPolyData::SafeDownCast(pickedActor->GetMapper()->GetInput());
        if (!polyData) return;

        // 获取这个三角形的顶点 ID
        vtkCell* cell = polyData->GetCell(pickedCellId);
        vtkIdList* pointIds = cell->GetPointIds();

        // 取出三角形的三个顶点索引
        vtkIdType v0 = pointIds->GetId(0);
        vtkIdType v1 = pointIds->GetId(1);
        vtkIdType v2 = pointIds->GetId(2);

        double pPos[3]{};
        picker->GetPCoords(pPos);
        pPos[2] = 1 - pPos[1] - pPos[0];

        std::array<double, 3> position1{};
        std::array<double, 3> position2{};
        vtkNew<vtkLine> line0;
        vtkNew<vtkPoints> points;
        vtkNew<vtkCellArray> lines;
        vtkSmartPointer<vtkPolyData> data;
        vtkPolyDataMapper* mapper = vtkPolyDataMapper::SafeDownCast(pickedActor->GetMapper());
        data = mapper->GetInput();
        vtkIdType* cellpid = data->GetCell(picker->GetCellId())->GetPointIds()->GetPointer(0);

        SelectedEdge picked_edge;
       
        /**/ if (pPos[1] < pPos[0] && pPos[1] < pPos[2]) {
            picked_edge.v_local_id[0] = v0;
            picked_edge.v_local_id[1] = v1;
        }
        else if (pPos[2] < pPos[0] && pPos[2] < pPos[1]) {
            picked_edge.v_local_id[0] = v1;
            picked_edge.v_local_id[1] = v2;
        }
        else if (pPos[0] < pPos[1] && pPos[0] < pPos[2]) {
            picked_edge.v_local_id[0] = v0;
            picked_edge.v_local_id[1] = v2;
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
        selected_mapper_->SetInputData(polydata);
        selected_mapper_->Update(); // 更新映射器

        // 创建一个演员来显示这些线段

        selected_actor_->SetMapper(selected_mapper_);
        selected_actor_->GetProperty()->SetColor(MeshActor::colors->GetColor3d("red").GetData());
        selected_actor_->GetProperty()->SetLineWidth(5); // 设置

        if (_is_selected(picked_edge, selection_, selected_actor_)) {
            _cancel_highlight(selected_mapper_, selected_actor_);
            selection_ = std::nullopt;
        }
        else
        {
            selection_ = picked_edge;
            renderer_->AddActor(selected_actor_);
            //renderer_->Render();
        }
    }

    else if (picker->GetCellId() == -1) {
        // 没选到
        _cancel_highlight(selected_mapper_, selected_actor_);
        selection_ = std::nullopt;
    }
}

vtkPropCollection* SingleEdgeSelectorHighlight::getPickList()
{
    return this->collection_;
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
