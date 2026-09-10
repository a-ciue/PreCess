#include "CreateLineFromVerticesHandler.h"
#include "ComponentData.h"
#include "GeometryData.h"
#include "FeatureContext.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "GeometryBuilder.h"

#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace systems::feature {
void CreateLineFromVerticesHandler::setup(FeatureRegistrar& reg, FeatureContext& /*ctx*/)
{
    // 参数声明与原 GeometryOperationActions.qml 中 createLineFromVerticesInfo 保持一致；
    // 菜单与图标复用原"几何"页"直线边（选点）"按钮的声明
    reg.addParameter({ ArgTypeEnum::Selector, "端点", "", "请选择两个几何点" });
    reg.addMenuItem({ "几何", "创建直线边（选择两点）", "qrc:/images/toolbar/Geometry/line_points.svg" });
}

std::any CreateLineFromVerticesHandler::execute(FeatureContext& ctx)
{
    // 依赖已有拓扑的操作写回其来源 Component（对象树选中态提示）
    const auto component_id = ctx.activeComponent ? ctx.activeComponent() : std::nullopt;
    if (!component_id)
        return std::string("请先选择目标 Component。");

    const auto* selection_param = ctx.params.value(0).get<ArgTypeEnum::Selector>();
    if (!selection_param || !*selection_param)
        return std::string("请选择两个几何点。");
    const std::shared_ptr<Selection>& selected = *selection_param;
    if (selected->type != ElementEnum::GeometryVertex || selected->ids.size() != 2)
        return std::string("请选择两个几何点。");

    try {
        ComponentData* component = ctx.model.findComponent(*component_id);
        if (!component || !component->geometry || !component->geometry->rootShape)
            return std::string("目标组件没有几何。");
        component->geometry->ensureIndexBuilt(ctx.model.geomRegistry());

        const auto& component_vertex_ids = component->geometry->index.vertex_local_to_global;
        for (Index id : selected->ids) {
            if (std::find(component_vertex_ids.begin(), component_vertex_ids.end(), id)
                == component_vertex_ids.end())
                return std::string("所选几何点必须属于目标组件。");
        }

        const TopoDS_Shape* start_shape = ctx.model.geomRegistry().getVertex(selected->ids[0]);
        const TopoDS_Shape* end_shape = ctx.model.geomRegistry().getVertex(selected->ids[1]);
        if (!start_shape || !end_shape
            || start_shape->ShapeType() != TopAbs_VERTEX
            || end_shape->ShapeType() != TopAbs_VERTEX)
            return std::string("所选几何点已失效。");

        // 在索引释放前复制 TopoDS_Vertex，随后用它们构造共享端点的 Edge。
        const TopoDS_Vertex start = TopoDS::Vertex(*start_shape);
        const TopoDS_Vertex end = TopoDS::Vertex(*end_shape);
        TopoDS_Shape line = GeometryBuilder::makeLine(start, end);
        auto component_operator = ctx.model.getComponentOperator(*component_id);
        if (!component_operator)
            return std::string("几何操作失败，详细原因请查看日志。");
        ++next_line_number_;
        return component_operator->appendGeometryShape(std::move(line));
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("CreateLineFromVertices: {}", detail ? detail : "OpenCASCADE error");
    } catch (const std::exception& error) {
        spdlog::error("CreateLineFromVertices: {}", error.what());
    }
    return std::string("几何操作失败，详细原因请查看日志。");
}
}
