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
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkType.h>

class vtkRenderer;
class vtkActor;
class vtkProp;
class vtkMapper;
class vtkHardwarePicker;
class vtkDataSet;
class vtkCell;
class vtkPartitionedDataSet;
class vtkExtractSelection;
class vtkGeometryFilter;
class vtkPolyData;
class vtkCompositePolyDataMapper;
class IMeshIdQuery;

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
    //! @brief 单点拾取：使用外层预 picker.Pick 拾取结果，避免两次 picker.Pick 污染 picking buffer。
    virtual void select(double posx, double posy,
        vtkHardwarePicker* picker, vtkActor* picked_actor,
        vtkIdType picked_cell_id, vtkIdType picked_point_id) = 0;
    //! @brief 应用一次多 actor 框选拾取结果到本选择器（MeshSelectManager 已先清空全部组件选择并分发命中）
    //! @param hits 各 actor(PROP) 在本框内的命中 render id 集合（本选择器按自身 actor 取用）
    //! @param xmin ymin xmax ymax  屏幕像素矩形（Edge/Vertex 屏幕投影二次过滤用）
    virtual void selectArea(
        const std::unordered_map<vtkProp*, std::unordered_set<vtkIdType>>& hits,
        int xmin, int ymin, int xmax, int ymax)
        = 0;
    /**
     * @brief 清空选中元素，并取消高亮
     */
    virtual void clear() = 0;
    /**
     * @brief 仅清除 partition 高亮数据，保留内部选择数据
     */
    virtual void disableHighlight() = 0;
    /**
     * @brief 从保留的选择数据重新下发 partition 高亮
     */
    virtual void enableHighlight() = 0;
    virtual SelectionVtk get() = 0;
};

class FaceSelectorHighlight : public SelectorHighlight {
public:
    static void setupHighlightStyle(vtkActor& actor, vtkMapper& mapper);

    FaceSelectorHighlight(vtkRenderer& renderer, vtkPartitionedDataSet& highlight_data,
        unsigned int partition_id, MeshActorSelectOp select_op);
    ~FaceSelectorHighlight() override;
    //! @brief 自建 picker 的点选兼容入口（测试/独立调用用；生产路径由 MeshSelectManager 预拾后调下面的 picker 重载）
    void select(double posx, double posy);
    void select(double posx, double posy,
        vtkHardwarePicker* picker, vtkActor* picked_actor,
        vtkIdType picked_cell_id, vtkIdType picked_point_id) override;
    void selectArea(
        const std::unordered_map<vtkProp*, std::unordered_set<vtkIdType>>& hits,
        int xmin, int ymin, int xmax, int ymax) override;
    void clear() override;
    void disableHighlight() override;
    void enableHighlight() override;
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
        unsigned int partition_id, MeshActorSelectOp select_op,
        Index component_id, const IMeshIdQuery* id_query);
    ~EdgeSelectorHighlight() override;
    //! @brief 自建 picker 的点选兼容入口（测试/独立调用用；生产路径由 MeshSelectManager 预拾后调下面的 picker 重载）
    void select(double posx, double posy);
    void select(double posx, double posy,
        vtkHardwarePicker* picker, vtkActor* picked_actor,
        vtkIdType picked_cell_id, vtkIdType picked_point_id) override;
    void selectArea(
        const std::unordered_map<vtkProp*, std::unordered_set<vtkIdType>>& hits,
        int xmin, int ymin, int xmax, int ymax) override;
    void clear() override;
    void disableHighlight() override;
    void enableHighlight() override;
    SelectionVtk get() override;

private:
    //! @brief 选中的一条边：端点对用于高亮，edge_id 记录稳定局部边 id
    struct SelectedEdge {
        std::array<vtkIdType, 2> endpoints; //> 局部点 id（高亮按端点对画线；get() 出口统一换算全局点 id）
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
    //! @brief 自建 picker 的点选兼容入口（测试/独立调用用；生产路径由 MeshSelectManager 预拾后调下面的 picker 重载）
    void select(double posx, double posy);
    void select(double posx, double posy,
        vtkHardwarePicker* picker, vtkActor* picked_actor,
        vtkIdType picked_cell_id, vtkIdType picked_point_id) override;
    void selectArea(
        const std::unordered_map<vtkProp*, std::unordered_set<vtkIdType>>& hits,
        int xmin, int ymin, int xmax, int ymax) override;
    void clear() override;
    void disableHighlight() override;
    void enableHighlight() override;
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
        unsigned int partition_id, MeshActorSelectOp select_op,
        Index component_id, const IMeshIdQuery* id_query);
    ~VertexSelectorHighlight() override;
    //! @brief 自建 picker 的点选兼容入口（测试/独立调用用；生产路径由 MeshSelectManager 预拾后调下面的 picker 重载）
    void select(double posx, double posy);
    void select(double posx, double posy,
        vtkHardwarePicker* picker, vtkActor* picked_actor,
        vtkIdType picked_cell_id, vtkIdType picked_point_id) override;
    void selectArea(
        const std::unordered_map<vtkProp*, std::unordered_set<vtkIdType>>& hits,
        int xmin, int ymin, int xmax, int ymax) override;
    void clear() override;
    void disableHighlight() override;
    void enableHighlight() override;
    SelectionVtk get() override;
    void selectPickedPoint(vtkDataSet* picked_data_set, vtkIdType picked_point_id);

private:
    vtkRenderer* renderer_;
    MeshActorSelectOp select_op_;
    vtkPartitionedDataSet* highlight_data_;
    unsigned int partition_id_;
    Index component_id_ { -1 };
    const IMeshIdQuery* id_query_ {};
    vtkNew<vtkIdTypeArray> selected_ids_; //> 选中的局部点 id，作为提取选择列表的活数组；get() 出口统一换算全局点 id
    vtkSmartPointer<vtkExtractSelection> extract_filter_;
    vtkNew<vtkGeometryFilter> geom_filter_;
};
#endif
