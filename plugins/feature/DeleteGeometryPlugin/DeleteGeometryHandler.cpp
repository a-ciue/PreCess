#include "DeleteGeometryHandler.h"
#include "ComponentData.h"
#include "GeometryData.h"
#include "ComponentOperator.h"
#include "FeatureContext.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "GeometryTopologyEditor.h"

#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS_Shape.hxx>
#include <spdlog/spdlog.h>

namespace systems::feature {
void DeleteGeometryHandler::setup(FeatureRegistrar& reg, FeatureContext& /*ctx*/)
{
    // 参数声明与原 GeometryOperationActions.qml 中 deleteGeometryInfo 保持一致；
    // 菜单与图标复用原"几何"页"删除几何"按钮的声明
    reg.addParameter({ ArgTypeEnum::Selector, "目标几何", "",
        "请在顶部选择器中切换几何点、边、面或体模式" });
    reg.addParameter({ ArgTypeEnum::Bool, "同时删除下级拓扑", "false",
        "关闭时保留直接下级拓扑，开启时不影响其他形状共享的拓扑" });
    reg.addMenuItem({ "几何", "删除几何", "qrc:/images/toolbar/Geometry/delete_geometry.svg" });
}

std::any DeleteGeometryHandler::execute(FeatureContext& ctx)
{
    const auto* selection_param = ctx.params.value(0).get<ArgTypeEnum::Selector>();
    if (!selection_param || !*selection_param)
        return std::string("请选择一个几何形状。");
    const std::shared_ptr<Selection>& selected = *selection_param;
    if (selected->ids.size() != 1)
        return std::string("请选择一个几何形状。");

    const bool* delete_children_param = ctx.params.value(1).get<ArgTypeEnum::Bool>();
    const bool delete_children = delete_children_param ? *delete_children_param : false;

    try {
        const Index shape_id = selected->ids.front();
        TopAbs_ShapeEnum shape_type = TopAbs_SHAPE;
        const TopoDS_Shape* selected_shape = nullptr;

        // 选择类型决定全局几何 ID 的类别以及对应的 OCC Shape 注册表。
        switch (selected->type) {
        case ElementEnum::GeometryVertex:
            shape_type = TopAbs_VERTEX;
            selected_shape = ctx.model.geomRegistry().getVertex(shape_id);
            break;
        case ElementEnum::GeometryEdge:
            shape_type = TopAbs_EDGE;
            selected_shape = ctx.model.geomRegistry().getEdge(shape_id);
            break;
        case ElementEnum::GeometryFace:
            shape_type = TopAbs_FACE;
            selected_shape = ctx.model.geomRegistry().getFace(shape_id);
            break;
        case ElementEnum::GeometrySolid:
            shape_type = TopAbs_SOLID;
            selected_shape = ctx.model.geomRegistry().getSolid(shape_id);
            break;
        default:
            return std::string("选择必须是几何点、边、面或体。");
        }

        if (!selected_shape)
            return std::string("所选几何形状已失效。");

        // 操作目标 Component 由所选形状反查得到，不依赖对象树选中态
        const auto component_id = ctx.model.findComponentIdByGeometryShapeId(shape_type, shape_id);
        if (!component_id)
            return std::string("所选几何形状不属于任何组件。");

        ComponentData* component = ctx.model.findComponent(*component_id);
        if (!component || !component->geometry || !component->geometry->rootShape)
            return std::string("目标组件没有几何。");

        // 在释放旧索引前复制 OCC Shape 句柄，并先完成纯 OCC 根拓扑重建。
        const TopoDS_Shape target = *selected_shape;
        TopoDS_Shape result = GeometryTopologyEditor::removeTopLevelShape(
            *component->geometry->rootShape, target, delete_children);

        auto component_operator = ctx.componentOperator
            ? ctx.componentOperator(*component_id)
            : std::nullopt;
        if (!component_operator)
            return std::string("目标组件没有几何。");

        // 替换几何根（写入即标脏），undo 记录与通知由 invoke 操作边界负责
        return component_operator->replaceGeometryRoot(std::move(result));
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("DeleteGeometry: {}", detail ? detail : "OpenCASCADE error");
    } catch (const std::exception& error) {
        spdlog::error("DeleteGeometry: {}", error.what());
    }
    return std::string("几何操作失败，详细原因请查看日志。");
}
}
