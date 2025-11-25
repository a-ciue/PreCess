#include "SelectorHighlight.h"
#include "MeshActor.h"
#include "MeshActorSelectOp.h"
#include "Selection.h"
#include <array>
#include <optional>
#include <utility>
#include <vector>
#include <vtkActor.h>
#include <vtkAppendPolyData.h>
#include <vtkAssemblyPath.h>
#include <vtkCellData.h>
#include <vtkCellPicker.h>
#include <vtkCompositeDataDisplayAttributes.h>
#include <vtkCompositePolyDataMapper.h>
#include <vtkDataSetMapper.h>
#include <vtkExtractSelection.h>
#include <vtkHardwarePicker.h>
#include <vtkLine.h>
#include <vtkMapper.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkNamedColors.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkPropAssembly.h>
#include <vtkPropPicker.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>

BlockSelectorHighlight::BlockSelectorHighlight(vtkRenderer* renderer)
{
    this->renderer_ = renderer;
}

void BlockSelectorHighlight::clear()
{
    // 取消高亮所有选中的actor
    for (auto& selection : selections_) {
        _cancel_highlight(selection);
    }
    selections_.clear();
}

SelectionVtk BlockSelectorHighlight::get()
{
    SelectionVtk back_selection;
    back_selection.type = ElementEnum::Block;
    for (const auto& selection : selections_) {
        back_selection.ids.push_back(selection.block_id);
    }
    return back_selection;
}

void BlockSelectorHighlight::select(double posx, double posy)
{
    vtkNew<vtkCellPicker> picker;
    picker->PickFromListOn();
    collection_->InitTraversal();
    for (vtkProp* actor {}; actor = collection_->GetNextProp();) {
        picker->AddPickList(actor);
    }

    picker->Pick(posx, posy, 0, this->renderer_);
    vtkProp* pickedProp = picker->GetViewProp();

    if (!pickedProp || !pickedProp->IsA("vtkActor")) {
        std::cout << "No valid object picked!" << std::endl;
        for (auto& selection : selections_) {
            _cancel_highlight(selection);
        }
        selections_.clear(); // 清空选择列表

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
    } else {
        // ✅ 新增高亮
        selections_.emplace_back();
        Block& sel = selections_.back();
        sel.block_id = blockIndex;
        auto multiblock = vtkMultiBlockDataSet::SafeDownCast(this->mapper_->GetInputDataObject(0, 0));
        if (!multiblock || blockIndex >= multiblock->GetNumberOfBlocks())
            return;

        auto block = vtkPolyData::SafeDownCast(multiblock->GetBlock(blockIndex));
        if (!block)
            return;

        auto colors = vtkUnsignedCharArray::SafeDownCast(block->GetCellData()->GetScalars());
        if (colors && colors->GetNumberOfTuples() > 0) {
            // ✅ 保存 block 第一个 cell 的原始颜色
            sel.backup_color[0] = colors->GetComponent(0, 0) / 255.0;
            sel.backup_color[1] = colors->GetComponent(0, 1) / 255.0;
            sel.backup_color[2] = colors->GetComponent(0, 2) / 255.0;
        } else {
            // 默认白色
            sel.backup_color[0] = 1.0;
            sel.backup_color[1] = 1.0;
            sel.backup_color[2] = 1.0;
        }

        highlightBlockByCellColor(this->mapper_, blockIndex, 255, 0, 0); // 高亮为红色
    }

    // 刷新 actor 显示
    actor->SetMapper(this->mapper_);
    this->renderer_->RemoveActor(actor); // 可选刷新方法
    this->renderer_->AddActor(actor);
}

void BlockSelectorHighlight::highlightBlockByCellColor(vtkCompositePolyDataMapper* mapper, unsigned int block_index,
    unsigned char r, unsigned char g, unsigned char b)
{
    auto multiblock = vtkMultiBlockDataSet::SafeDownCast(mapper->GetInputDataObject(0, 0));
    if (!multiblock || block_index >= multiblock->GetNumberOfBlocks())
        return;

    auto block = vtkPolyData::SafeDownCast(multiblock->GetBlock(block_index));
    if (!block)
        return;

    auto colors = vtkUnsignedCharArray::SafeDownCast(block->GetCellData()->GetScalars());
    if (!colors || colors->GetNumberOfComponents() != 3)
        return;

    for (vtkIdType i = 0; i < colors->GetNumberOfTuples(); ++i) {
        colors->SetTypedTuple(i, std::array<unsigned char, 3> { r, g, b }.data());
    }

    block->GetCellData()->SetScalars(colors);
    block->Modified(); // 通知 VTK 数据已更新
}

void BlockSelectorHighlight::setCurModelActor(MeshActorSelectOpFactory model_actor)
{
    this->collection_->RemoveAllItems();
    if (auto actor = model_actor.lock()) {
        this->collection_->AddItem(&actor->getBlockActor());
    }
    this->model_actor_ = model_actor;
}

void BlockSelectorHighlight::_cancel_highlight(Block& selection)
{
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
        colors->SetTypedTuple(i, std::array<unsigned char, 3> { r, g, b }.data());
    }

    // 更新 block 数据
    block->GetCellData()->SetScalars(colors);
    block->Modified(); // 通知 VTK 数据已更改，进行刷新
}

std::optional<size_t> BlockSelectorHighlight::_is_selected(const vtkIdType block_id, const std::vector<Block>& selections)
{

    for (size_t i = 0; i < selections.size(); ++i) {
        if (selections[i].block_id == block_id) {
            // 如果找到,返回该 actor 的索引
            return i;
        }
    }
    return std::nullopt;
}

