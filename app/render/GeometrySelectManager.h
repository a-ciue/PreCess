#ifndef GEOMETRY_SELECT_MANAGER_H
#define GEOMETRY_SELECT_MANAGER_H
#include "Core.h"
#include "Selection.h"
#include "GeometrySelectorHighlight.h"
#include "GeometryActorSelectOp.h"

#include <memory>
#include <unordered_map>
#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkActor.h>

class vtkRenderer;
class vtkPartitionedDataSet;
class vtkCompositePolyDataMapper;
class GeometryActorManagerSelectOp;

class GeometrySelectManager {
public:
    GeometrySelectManager(vtkRenderer& renderer, vtkActor& highlight_actor, GeometryActorManagerSelectOp& op);

    void select(double posx, double posy);
    void setSelectMode(SelectMode select_mode);
    void clearSelection();
    std::unique_ptr<Selection> getSelection();

    /**
     * @brief 进入/退出几何顶点吸附模式（交互服务开始/结束时调用）
     *
     * 内部把拾取模式切到几何顶点/还原，调用方无需感知 picker 的存在。
     * 与选择互斥由 UI 层保证（交互激活期间选择模式为 None）。
     */
    void setVertexSnapActive(bool on);
    /**
     * @brief 吸附几何顶点：拾取并解析为几何顶点
     * @return 命中时返回 {GeometryRegistry 顶点 id, 世界坐标}，未命中返回 std::nullopt
     */
    std::optional<std::pair<Index, std::array<double, 3>>> snapGeometryVertex(double posx, double posy);

private:
    GeometrySelectorHighlight* getOrCreateSelector(Index component_id);

    GeometryActorManagerSelectOp* op_;
    SelectMode select_mode_ { SelectMode::None };
    vtkRenderer* renderer_;
    vtkSmartPointer<IVtkTools_ShapePicker> picker_;

    vtkActor* highlight_actor_ {};
    vtkSmartPointer<vtkPartitionedDataSet> highlight_data_;
    vtkSmartPointer<vtkCompositePolyDataMapper> highlight_mapper_;

    std::unordered_map<Index, std::unique_ptr<GeometrySelectorHighlight>> component_selectors_;
};

#endif
