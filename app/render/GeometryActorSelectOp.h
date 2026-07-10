#ifndef GEOMETRY_ACTOR_SELECT_OP_H
#define GEOMETRY_ACTOR_SELECT_OP_H
#include <memory>
#include <optional>
#include <vector>

#include <IVtk_Types.hxx>

#include "Core.h"
#include "GeometryActor.h"

class GeometryActorSelectOp;
class IVtkTools_SubPolyDataFilter;
class IVtkTools_ShapePicker;
class vtkActor;
class vtkRenderer;

class GeometryActorSelectOpFactory {
    friend GeometryActorSelectOp;

public:
    GeometryActorSelectOpFactory();
    GeometryActorSelectOpFactory(std::weak_ptr<GeometryActor> geometry_actor);
    std::optional<GeometryActorSelectOp> lock();

private:
    std::weak_ptr<GeometryActor> geometry_actor_;
};

class GeometryActorSelectOp {
    friend GeometryActorSelectOpFactory;

public:
    GeometryActorSelectOp(std::shared_ptr<GeometryActor> geometry_actor);

    // 重置该 actor 在 picker 上的所有 selection mode，并清空拾取列表
    void disablePickerModes(IVtkTools_ShapePicker* picker);

    // 按选择模式配置 picker：拾取列表 / 容差 / 启用的 selection mode
    void configurePicker(IVtkTools_ShapePicker* picker, SelectMode mode);

    // 面/边/点拾取：命中则返回几何全局 id，并通过 out_sub_id 输出 OCC 子形状 id
    std::optional<Index> pickSubshape(IVtkTools_ShapePicker* picker, vtkRenderer* renderer,
        double posx, double posy, SelectMode mode, IVtk_IdType& out_sub_id);

    // 体拾取：命中则输出体全局 id 及该体所有面的子形状 id
    bool pickSolid(IVtkTools_ShapePicker* picker, vtkRenderer* renderer, double posx, double posy,
        GeomSolidId& out_solid_id, std::vector<IVtk_IdType>& out_face_sub_ids);

    // 按选择模式返回对应的高亮管线（面/体用 poly，边/点用 line）
    IVtkTools_SubPolyDataFilter* highlightFilter(SelectMode mode);
    vtkActor* highlightActor(SelectMode mode);

private:
    std::shared_ptr<GeometryActor> geometry_actor_;
};
#endif // GEOMETRY_ACTOR_SELECT_OP_H
