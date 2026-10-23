#ifndef GEOMETRY_ACTOR_H
#define GEOMETRY_ACTOR_H
#include "Core.h"
#include "GeometryDataVtk.h"
#include "GeometrySubshapeIndex.h"
#include "GeometryTopologyDiagnosticActor.h"
#include "SurfacePrecision.h"
#include <vtkActor.h>
#include <vtkDataArray.h>
#include <vtkNew.h>
#include <vtkSmartPointer.h>

#include <IVtkOCC_Shape.hxx>
#include <Standard_Handle.hxx>
#include <memory>
typedef Handle(IVtkOCC_Shape) OccShapeHandle;

class GeometryActorSelectOp;
class GeometryTopologyDiagnosticActor;
class vtkPolyData;

class GeometryActor {
    friend GeometryActorSelectOp;
    friend class GeometryActorManager;

public:
    GeometryActor(vtkRenderer* renderer);
    ~GeometryActor();

    void loadShape(const GeometryDataVtk& geometry_data, int precision = surface_precision::kDefault);

    void setVisibility(bool visibility);
    bool isVisible() const;
    void setRenderStyle(GeometryRenderStyle style);
    GeometryRenderStyle getRenderStyle() const;

    /** @brief 获取本组件独立的几何拓扑诊断渲染对象。 */
    GeometryTopologyDiagnosticActor& topologyDiagnostics();

private:
    void deleteGeometryActor();
    void applyStyle();
    /** @brief 仅含显示数据；候选与回滚快照均不创建场景、拾取或诊断对象。 */
    struct DisplayData {
        vtkSmartPointer<vtkPolyData> lines;
        vtkSmartPointer<vtkPolyData> polygons;
        GeometryTopologyDiagnosticActor::DisplayData diagnostics;
    };
    DisplayData prepareDisplay(int precision, bool include_diagnostics = true) const;
    DisplayData captureDisplay() const;
    /** @brief 原位更新数据对象，保留 Mapper、高亮过滤器及 Picker 的身份。 */
    void commitDisplay(const DisplayData& display);
    void createActors();

    GeometryRenderStyle style_ { GeometryRenderStyle::SurfaceWithEdges };
    bool visibility_;
    OccShapeHandle occ_shape_;
    // 固定标准精度的拾取副本，与模型/撤销快照及可变显示三角化隔离。
    std::unique_ptr<GeometrySubshapeIndex> picking_geometry_index_;

    vtkSmartPointer<vtkActor> poly_actor_ = vtkSmartPointer<vtkActor>::New();
    vtkSmartPointer<vtkActor> line_actor_ = vtkSmartPointer<vtkActor>::New();
    vtkSmartPointer<vtkRenderer> renderer_;

    vtkSmartPointer<vtkPolyData> poly_only_;
    vtkSmartPointer<vtkPolyData> line_only_;
    std::unique_ptr<GeometryTopologyDiagnosticActor> topology_diagnostics_;
};

#endif
