#ifndef SELECT_MANAGER_H
#define SELECT_MANAGER_H
#include "Core.h"
#include "Selection.h"

#include <memory>
#include <string>
#include <vtkNew.h>
#include <vtkActor.h>
#include <vtkPolyDataMapper.h>

class vtkRenderer;
class MeshActor;
class GeometryActor;
class MeshSelectManager;
class GeometrySelectManager;

class SelectManager {
public:
    SelectManager();
    ~SelectManager();

    void bindRenderer(vtkRenderer* renderer);
    void select(double posx, double posy);

    void setSelectActor(std::weak_ptr<MeshActor> mesh_actor);
    void setSelectActor(std::weak_ptr<GeometryActor> geom_actor);

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