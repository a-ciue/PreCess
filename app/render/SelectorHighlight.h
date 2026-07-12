/**
* @file：SelectorHighlight.h
* @brief：渲染窗口中的选择和高亮操作
* @author：付轩宇 email 982531420@qq.com

*/
#ifndef SELECTOR_HIGHLIGHT_H
#define SELECTOR_HIGHLIGHT_H
#include "MeshActorSelectOp.h"
#include "Selection.h"

#include <array>
#include <optional>
#include <vector>
#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkAssembly.h>
#include <vtkCompositeDataDisplayAttributes.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkCompositePolyDataMapper.h>

class vtkRenderer;
class vtkDataSetMapper;
class vtkActor;
class vtkHardwarePicker;
class vtkCell;

using SelectionVtk = Selection;

class SelectorHighlight {
public:
    virtual ~SelectorHighlight() = default;
    virtual void select(double posx, double posy) = 0;
    /**
     * @brief 清空选中元素，并取消高亮
     */
    virtual void clear() = 0;
    virtual SelectionVtk get() = 0;
};

class BlockSelectorHighlight : public SelectorHighlight {
public:
    BlockSelectorHighlight(vtkRenderer& renderer, vtkActor& highlight_actor, MeshActorSelectOp select_op);
    ~BlockSelectorHighlight() override { clear(); }
    //! @brief 清空selections并取消高亮
    void clear()override;
    SelectionVtk get()override;
    void select(double posx, double posy) override;
    void highlightBlockByCellColor(vtkCompositePolyDataMapper* mapper, unsigned int block_index,
        unsigned char r, unsigned char g, unsigned char b);

private:
    //! @brief 存储选中的actor和每个actor原本的颜色渲染设置，用于取消高亮
    struct Block{
        vtkIdType block_id;
        double backup_color[3]{};
    };
    std::vector<Block> selections_;
    vtkRenderer* renderer_;
    vtkActor* highlight_actor_;
    MeshActorSelectOp select_op_;
    vtkCompositePolyDataMapper* mapper_;
    void _cancel_highlight(Block &selection);
    static std::optional<size_t> _is_selected(const vtkIdType block_id, const std::vector<Block>& selections);
};

class FaceSelectorHighlight : public SelectorHighlight {
public:
    FaceSelectorHighlight(vtkRenderer& renderer, vtkActor& highlight_actor, MeshActorSelectOp select_op);
    ~FaceSelectorHighlight() override;
    void select(double posx, double posy) override;
    void clear() override;
    SelectionVtk get() override;

private:
    vtkRenderer* renderer_;
    vtkActor* highlight_actor_;
    MeshActorSelectOp select_op_;
    std::vector<vtkIdType> selections_;
    vtkNew<vtkDataSetMapper> selected_mapper_;
};

class EdgeSelectorHighlight : public SelectorHighlight {
public:
    EdgeSelectorHighlight(vtkRenderer& renderer, vtkActor& highlight_actor, MeshActorSelectOp select_op);
    ~EdgeSelectorHighlight() override;
    void select(double posx, double posy) override;
    void clear() override;
    SelectionVtk get() override;

private:
    vtkRenderer* renderer_;
    vtkActor* highlight_actor_;
    MeshActorSelectOp select_op_;
    std::vector<std::array<vtkIdType, 2>> selections_;
    vtkNew<vtkDataSetMapper> selected_mapper_;
};

class SolidSelectorHighlight : public SelectorHighlight {
public:
    SolidSelectorHighlight(vtkRenderer& renderer, vtkActor& highlight_actor, MeshActorSelectOp select_op);
    ~SolidSelectorHighlight() override;
    void select(double posx, double posy) override;
    void clear() override;
    SelectionVtk get() override;

private:
    vtkRenderer* renderer_;
    vtkActor* highlight_actor_;
    MeshActorSelectOp select_op_;
    vtkSmartPointer<vtkDataSetMapper> mapper_;
    vtkNew<vtkIdTypeArray> selected_ids_; //> 存储选中的体id，绑定到了mapper，用于触发高亮体cell修改
};

class VertexSelectorHighlight : public SelectorHighlight {
public:
    VertexSelectorHighlight(vtkRenderer& renderer, vtkActor& highlight_actor, MeshActorSelectOp select_op);
    ~VertexSelectorHighlight() override;
    void select(double posx, double posy) override;
    void clear() override;
    SelectionVtk get() override;

private:
    vtkRenderer* renderer_;
    vtkActor* highlight_actor_ {};
    MeshActorSelectOp select_op_;
    vtkSmartPointer<vtkDataSetMapper> mapper_;
    vtkNew<vtkIdTypeArray> selected_ids_; //> 存储选中的点id，绑定到了mapper，用于触发高亮顶点修改
};
#endif
