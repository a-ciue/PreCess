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
#include <vtkType.h>

class vtkRenderer;
class vtkActor;
class vtkMapper;
class vtkHardwarePicker;
class vtkCell;
class vtkPartitionedDataSet;
class vtkExtractSelection;
class vtkGeometryFilter;
class vtkPolyData;

using SelectionVtk = Selection;

/**
 * @brief 面选择的角度扩散配置，用于控制是否沿共享边扩展到相邻面
 */
struct FaceSelectionSpreadOptions {
    bool enabled { false };
    double angle_deg { 30.0 };
};

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

class FaceSelectorHighlight : public SelectorHighlight {
public:
    static void setupHighlightStyle(vtkActor& actor, vtkMapper& mapper);

    FaceSelectorHighlight(vtkRenderer& renderer, vtkPartitionedDataSet& highlight_data,
        unsigned int partition_id, MeshActorSelectOp select_op);
    ~FaceSelectorHighlight() override;
    void select(double posx, double posy) override;
    void clear() override;
    SelectionVtk get() override;

    /**
     * @brief 设置面选择的扩散开关和角度阈值
     * @param options 面选择扩散配置
     */
    void setSpreadOptions(FaceSelectionSpreadOptions options);

private:
    /**
     * @brief 缓存当前面数据的邻接关系和法向，避免每次点击全量重建
     */
    struct FaceSpreadCache {
        vtkPolyData* poly_data {};
        vtkMTimeType mtime {};
        std::vector<std::vector<vtkIdType>> adjacency;
        std::vector<std::array<double, 3>> normals;
    };

    /**
     * @brief 当面数据发生变化时重建角度扩散缓存
     * @param poly 当前选择器使用的面数据
     */
    void updateSpreadCache(vtkPolyData& poly);

    vtkRenderer* renderer_;
    MeshActorSelectOp select_op_;
    vtkPartitionedDataSet* highlight_data_;
    unsigned int partition_id_;
    std::vector<vtkIdType> selections_;
    FaceSelectionSpreadOptions spread_options_;
    FaceSpreadCache spread_cache_;
    vtkSmartPointer<vtkPolyData> selections_poly_;
};

class EdgeSelectorHighlight : public SelectorHighlight {
public:
    static void setupHighlightStyle(vtkActor& actor, vtkMapper& mapper);

    EdgeSelectorHighlight(vtkRenderer& renderer, vtkPartitionedDataSet& highlight_data,
        unsigned int partition_id, MeshActorSelectOp select_op);
    ~EdgeSelectorHighlight() override;
    void select(double posx, double posy) override;
    void clear() override;
    SelectionVtk get() override;

private:
    vtkRenderer* renderer_;
    MeshActorSelectOp select_op_;
    vtkPartitionedDataSet* highlight_data_;
    unsigned int partition_id_;
    std::vector<std::array<vtkIdType, 2>> selections_;
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
