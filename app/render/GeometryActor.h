#ifndef GEOMETRY_ACTOR_H
#define GEOMETRY_ACTOR_H
#include "Core.h"
#include "GeometryDataVtk.h"
#include "GeometrySubshapeIndex.h"
#include "GeometryTopologyDiagnosticActor.h"
#include "SurfacePrecision.h"
#include <vtkActor.h>
#include <vtkNew.h>
#include <vtkSmartPointer.h>

#include <IVtkOCC_Shape.hxx>
#include <Standard_Handle.hxx>
#include <memory>
typedef Handle(IVtkOCC_Shape) OccShapeHandle;

class GeometryActorSelectOp;
class GeometryTopologyDiagnosticActor;
class vtkPolyData;
class vtkRenderer;

class GeometryActor {
    friend GeometryActorSelectOp;

public:
    GeometryActor(vtkRenderer* renderer);
    ~GeometryActor();

    void loadShape(const GeometryDataVtk& geometry_data, int precision = surface_precision::kDefault);
    /** @brief 重建私有拓扑的显示网格；调用侧负责清空选择及更新 Picker 注册。 */
    void setSurfacePrecision(int precision);
    int getSurfacePrecision() const { return surface_precision_; }

    void setVisibility(bool visibility);
    bool isVisible() const;
    void setRenderStyle(GeometryRenderStyle style);
    GeometryRenderStyle getRenderStyle() const;

    /** @brief 获取本组件独立的几何拓扑诊断渲染对象。 */
    GeometryTopologyDiagnosticActor& topologyDiagnostics();

private:
    void deleteGeometryActor();
    void applyStyle();
    void rebuildMesh(int precision);
    void createActors();

    GeometryRenderStyle style_ { GeometryRenderStyle::SurfaceWithEdges };
    bool visibility_;
    OccShapeHandle occ_shape_;
    // 显示和拾取共用私有拓扑；业务身份通过复制历史映射。
    std::unique_ptr<GeometrySubshapeIndex> picking_geometry_index_;

    vtkNew<vtkActor> poly_actor_;
    vtkNew<vtkActor> line_actor_;
    // 场景先销毁几何管理器，再释放 renderer。
    vtkRenderer* renderer_;
    int surface_precision_ { -1 }; //> 仅完整更新成功后记录档位，失败组件可重试。

    vtkSmartPointer<vtkPolyData> poly_only_;
    vtkSmartPointer<vtkPolyData> line_only_;
    std::unique_ptr<GeometryTopologyDiagnosticActor> topology_diagnostics_;
};

#endif
