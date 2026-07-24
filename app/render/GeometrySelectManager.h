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
    GeometrySelectManager(vtkRenderer& renderer, GeometryActorManagerSelectOp& op);

    void select(double posx, double posy);
    void setSelectMode(SelectMode select_mode);
    void clearSelection();
    std::unique_ptr<Selection> getSelection();
    void setHighlightVisible(bool visible);
    void setHighlightVisible(Index component_id, bool visible);

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

    vtkNew<vtkActor> highlight_actor_;
    vtkSmartPointer<vtkPartitionedDataSet> highlight_data_;
    vtkSmartPointer<vtkCompositePolyDataMapper> highlight_mapper_;

    std::unordered_map<Index, std::unique_ptr<GeometrySelectorHighlight>> component_selectors_;
};

#endif
