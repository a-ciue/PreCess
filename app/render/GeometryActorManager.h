#ifndef GEOMETRY_ACTOR_MANAGER_H
#define GEOMETRY_ACTOR_MANAGER_H
#include "Core.h"
#include "GeometryActorManagerSelectOp.h"
#include "GeometryTopologyDiagnosticCategory.h"
#include <array>
#include <memory>
#include <unordered_map>
#include <vector>
#include <vtkRenderer.h>

struct GeometryDataVtk;
class GeometryActor;

class GeometryActorManager {
public:
    GeometryActorManager();
    ~GeometryActorManager();
    void bindRender(vtkRenderer* renderer);

    std::shared_ptr<GeometryActor> getComponentActor(Index component_id) const;
    bool hasComponent(Index component_id) const;

    void deleteComponent(Index component_id);
    void loadGeometry(const GeometryDataVtk& geometry_data);

    void setVisibility(Index component_id, bool visibility);
    void setCurrentRenderStyle(GeometryRenderStyle style);
    GeometryRenderStyle getCurrentRenderStyle() const;

    /** @brief 设置窗口级几何拓扑诊断类别是否启用。 */
    void setTopologyDiagnosticCategoryEnabled(int category, bool enabled);
    /** @brief 设置窗口级几何清理容差。 */
    void setTopologyDiagnosticTolerance(double tolerance);

    GeometryActorManagerSelectOp& op() { return op_; }
    const GeometryActorManagerSelectOp& op() const { return op_; }

private:
    GeometryActorManagerSelectOp op_{*this};
    std::unordered_map<Index, std::shared_ptr<GeometryActor>> component_actors_;
    vtkRenderer* renderer_;
    GeometryRenderStyle current_style_ { GeometryRenderStyle::SurfaceWithEdges };
    std::array<bool, kGeometryTopologyDiagnosticCategoryCount> topology_diagnostic_category_enabled_ {};
    double topology_diagnostic_tolerance_ { 1.0e-6 };
};
#endif
