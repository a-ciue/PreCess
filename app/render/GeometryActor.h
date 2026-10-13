#ifndef GEOMETRY_ACTOR_H
#define GEOMETRY_ACTOR_H
#include "Core.h"
#include "GeometryDataVtk.h"
#include "GeometrySubshapeIndex.h"
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

    void loadShape(const GeometryDataVtk& geometry_data);

    /** @brief 设置显示用曲面精细度：0 粗糙、1 标准、2 精细、3 非常精细。 */
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
    void rebuildShape(int precision);
    /** @brief 在独立 renderer 与拓扑副本上准备切档，失败不触碰当前显示。 */
    std::unique_ptr<GeometryActor> prepareSurfacePrecision(int precision) const;
    /** @brief 交换已准备好的显示数据，保留供选择高亮引用的 GeometryActor 身份。 */
    void swapDisplay(GeometryActor& other);

    GeometryRenderStyle style_ { GeometryRenderStyle::SurfaceWithEdges };
    int surface_precision_ { surface_precision::kDefault };
    bool visibility_;
    OccShapeHandle occ_shape_;
    // 显示拓扑独立持有三角缓存，gid 沿用原模型，不注册到业务 GeometryRegistry。
    std::unique_ptr<GeometrySubshapeIndex> display_geometry_index_;
    const GeometrySubshapeIndex* geometry_index_ { nullptr };

    vtkSmartPointer<vtkActor> poly_actor_ = vtkSmartPointer<vtkActor>::New();
    vtkSmartPointer<vtkActor> line_actor_ = vtkSmartPointer<vtkActor>::New();
    vtkSmartPointer<vtkRenderer> renderer_;
    vtkSmartPointer<vtkDataArray> line_sub_id_array_;
    vtkSmartPointer<vtkDataArray> poly_sub_id_array_;

    vtkSmartPointer<vtkPolyData> poly_only_;
    vtkSmartPointer<vtkPolyData> line_only_;
    std::unique_ptr<GeometryTopologyDiagnosticActor> topology_diagnostics_;
};

#endif
