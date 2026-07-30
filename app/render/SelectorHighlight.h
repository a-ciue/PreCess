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

class vtkRenderer;
class vtkActor;
class vtkMapper;
class vtkHardwarePicker;
class vtkCell;
class vtkPartitionedDataSet;
class vtkExtractSelection;
class vtkGeometryFilter;
class vtkCompositePolyDataMapper;
class IMeshIdQuery;

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
    void clear() override;
    SelectionVtk get() override;
    void select(double posx, double posy) override;
    void highlightBlockByCellColor(vtkCompositePolyDataMapper* mapper, unsigned int block_index,
        unsigned char r, unsigned char g, unsigned char b);

private:
    //! @brief 存储选中的actor和每个actor原本的颜色渲染设置，用于取消高亮
    struct Block {
        vtkIdType block_id;
        double backup_color[3] { };
    };
    std::vector<Block> selections_;
    vtkRenderer* renderer_;
    MeshActorSelectOp select_op_;
    vtkCompositePolyDataMapper* mapper_;
    void _cancel_highlight(Block& selection);
    static std::optional<size_t> _is_selected(const vtkIdType block_id, const std::vector<Block>& selections);
};

class FaceSelectorHighlight : public SelectorHighlight {
public:
    static void setupHighlightStyle(vtkActor& actor, vtkMapper& mapper);

    FaceSelectorHighlight(vtkRenderer& renderer, vtkPartitionedDataSet& highlight_data,
        unsigned int partition_id, MeshActorSelectOp select_op);
    ~FaceSelectorHighlight() override;
    void select(double posx, double posy) override;
    void clear() override;
    SelectionVtk get() override;

private:
    vtkRenderer* renderer_;
    MeshActorSelectOp select_op_;
    vtkPartitionedDataSet* highlight_data_;
    unsigned int partition_id_;
    std::vector<vtkIdType> selections_;
    vtkSmartPointer<vtkPolyData> selections_poly_;
};

class EdgeSelectorHighlight : public SelectorHighlight {
public:
    static void setupHighlightStyle(vtkActor& actor, vtkMapper& mapper);

    EdgeSelectorHighlight(vtkRenderer& renderer, vtkPartitionedDataSet& highlight_data,
        unsigned int partition_id, MeshActorSelectOp select_op,
        Index component_id, const IMeshIdQuery* id_query);
    ~EdgeSelectorHighlight() override;
    void select(double posx, double posy) override;
    void clear() override;
    SelectionVtk get() override;

private:
    //! @brief 选中的一条边：端点对用于高亮，edge_id 记录稳定局部边 id
    struct SelectedEdge {
        std::array<vtkIdType, 2> endpoints; //> 全局点 id（高亮按端点对画线）
        Index edge_id { -1 }; //> 稳定局部边 id；id 查询缺失时为 -1
    };

    vtkRenderer* renderer_;
    MeshActorSelectOp select_op_;
    vtkPartitionedDataSet* highlight_data_;
    unsigned int partition_id_;
    Index component_id_ { -1 };
    const IMeshIdQuery* id_query_ {};
    std::vector<SelectedEdge> selections_;
    vtkNew<vtkPolyData> selections_poly_;
};

class SolidSelectorHighlight : public SelectorHighlight {
public:
    static void setupHighlightStyle(vtkActor& actor, vtkMapper& mapper);

    SolidSelectorHighlight(vtkRenderer& renderer, vtkPartitionedDataSet& highlight_data,
        unsigned int partition_id, MeshActorSelectOp select_op);
    ~SolidSelectorHighlight() override;
    void select(double posx, double posy) override;
    void clear() override;
    SelectionVtk get() override;

private:
    vtkRenderer* renderer_;
    MeshActorSelectOp select_op_;
    vtkPartitionedDataSet* highlight_data_;
    unsigned int partition_id_;
    vtkNew<vtkIdTypeArray> selected_ids_; //> 存储选中的体id，绑定到了mapper，用于触发高亮体cell修改
    vtkSmartPointer<vtkExtractSelection> extract_filter_;
    vtkNew<vtkGeometryFilter> geom_filter_;
};

class VertexSelectorHighlight : public SelectorHighlight {
public:
    static void setupHighlightStyle(vtkActor& actor, vtkMapper& mapper);

    VertexSelectorHighlight(vtkRenderer& renderer, vtkPartitionedDataSet& highlight_data,
        unsigned int partition_id, MeshActorSelectOp select_op);
    ~VertexSelectorHighlight() override;
    void select(double posx, double posy) override;
    void clear() override;
    SelectionVtk get() override;

private:
    vtkRenderer* renderer_;
    MeshActorSelectOp select_op_;
    vtkPartitionedDataSet* highlight_data_;
    unsigned int partition_id_;
    vtkNew<vtkIdTypeArray> selected_ids_; //> 存储选中的点id，绑定到了mapper，用于触发高亮顶点修改
    vtkSmartPointer<vtkExtractSelection> extract_filter_;
    vtkNew<vtkGeometryFilter> geom_filter_;
};
#endif
