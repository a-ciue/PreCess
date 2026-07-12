#ifndef SELECT_MANAGER_H
#define SELECT_MANAGER_H
#include "Core.h"
#include "Selection.h"

#include <memory>
#include <string>
#include <vtkActor.h>
#include <vtkNew.h>
#include <vtkPolyDataMapper.h>

class vtkRenderer;
class MeshSelectManager;
class GeometrySelectManager;
class MeshActorManagerSelectOp;
class GeometryActorManagerSelectOp;

class SelectManager {
public:
    SelectManager();
    ~SelectManager();

    void bindRenderer(vtkRenderer* renderer);
    void setOps(MeshActorManagerSelectOp& meshOp, GeometryActorManagerSelectOp& geomOp);

    void select(double posx, double posy);
    void setSelectMode(const std::string& select_mode);
    void clearSelection();
    std::unique_ptr<Selection> getSelection();

private:
    std::unique_ptr<MeshSelectManager> mesh_;
    std::unique_ptr<GeometrySelectManager> geom_;
    vtkNew<vtkActor> highlight_actor_;
    vtkNew<vtkPolyDataMapper> empty_mapper_;
};
#endif