#include "CollapseEdgeHandler.h"
#include "ComponentData.h"
#include "ComponentOperator.h"
#include "FeatureContext.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "GeometryData.h"
#include "GeometryTopologyEditor.h"
#include <BRep_Tool.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Pnt.hxx>
#include <spdlog/spdlog.h>
#include <exception>
#include <optional>
#include <string>
#include <utility>

namespace systems::feature {
void CollapseEdgeHandler::setup(FeatureRegistrar& reg, FeatureContext& /*ctx*/)
{
    reg.addParameter({ ArgTypeEnum::Selector, "目标边", "GeometryEdge", "请选择一条需要压缩的几何边" });
    reg.addParameter({ ArgTypeEnum::Combo, "目标位置", "默认,起点,终点,中点|0", "自动选择优先保留约束更多、修改风险更高的端点" });
    reg.navigation().addEntry({ "CollapseEdge", "压缩边", "", 0, "几何/拓扑" });
}

std::any CollapseEdgeHandler::execute(FeatureContext& ctx)
{
    const auto* selection = ctx.params.value(0).get<ArgTypeEnum::Selector>();
    const auto* target = ctx.params.value(1).get<ArgTypeEnum::Combo>();
    if (!selection || !*selection || (*selection)->type != ElementEnum::GeometryEdge
        || (*selection)->ids.size() != 1)
        return std::string("请选择一条需要压缩的几何边。");
    if (!target || *target < 0 || *target > 3)
        return std::string("请选择有效的压缩目标位置。");
    try {
        const Index edge_id = (*selection)->ids.front();
        const auto component_id = ctx.model.findComponentIdByGeometryShapeId(TopAbs_EDGE, edge_id);
        const TopoDS_Shape* shape = ctx.model.geomRegistry().getEdge(edge_id);
        if (!component_id || !shape)
            return std::string("所选几何边已失效。");
        ComponentData* component = ctx.model.findComponent(*component_id);
        if (!component || !component->geometry || !component->geometry->rootShape)
            return std::string("目标组件没有几何。");
        if (component->mapping && !component->mapping->empty())
            return std::string("目标组件已经建立几何-网格映射，不能修改几何拓扑。");
        const TopoDS_Edge selected_edge = TopoDS::Edge(*shape);
        TopoDS_Vertex start;
        TopoDS_Vertex end;
        TopExp::Vertices(selected_edge, start, end, true);
        if (start.IsNull() || end.IsNull())
            return std::string("所选几何边没有两个有效端点。");

        gp_Pnt target_position;
        if (*target == 0) {
            target_position = BRep_Tool::Pnt(
                GeometryTopologyEditor::recommendCollapseVertex(
                    *component->geometry->rootShape, selected_edge));
        } else if (*target == 1) {
            target_position = BRep_Tool::Pnt(start);
        } else if (*target == 2) {
            target_position = BRep_Tool::Pnt(end);
        } else {
            const gp_Pnt start_point = BRep_Tool::Pnt(start);
            const gp_Pnt end_point = BRep_Tool::Pnt(end);
            target_position.SetCoord(
                (start_point.X() + end_point.X()) * 0.5,
                (start_point.Y() + end_point.Y()) * 0.5,
                (start_point.Z() + end_point.Z()) * 0.5);
        }
        TopoDS_Shape result = GeometryTopologyEditor::collapseEdge(
            *component->geometry->rootShape, selected_edge, target_position);
        auto op = ctx.componentOperator(*component_id);
        if (!op)
            return std::string("几何操作失败，详细原因请查看日志。");
        op->replaceGeometryRoot(std::move(result));
        return std::string("压缩边成功");
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("CollapseEdge: {}", detail ? detail : "OpenCASCADE error");
        return std::string("几何边压缩失败，详细原因请查看日志。");
    } catch (const std::exception& error) {
        spdlog::error("CollapseEdge: {}", error.what());
        return std::string("几何边压缩失败，详细原因请查看日志。");
    }
}
}
