#include "MergeEdgeHandler.h"
#include "ComponentData.h"
#include "ComponentOperator.h"
#include "FeatureContext.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "GeometryData.h"
#include "GeometryTopologyEditor.h"

#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <spdlog/spdlog.h>

#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace systems::feature {
void MergeEdgeHandler::setup(FeatureRegistrar& reg, FeatureContext& /*ctx*/)
{
    reg.addParameter({ ArgTypeEnum::Selector, "待合并边", "GeometryEdge", "请选择两条或多条连续且同域的几何边" });
    reg.navigation().addEntry({ "MergeEdge", "合并边", "", 0, "几何/拓扑" });
}

std::any MergeEdgeHandler::execute(FeatureContext& ctx)
{
    const auto* edge_param = ctx.params.value(0).get<ArgTypeEnum::Selector>();
    if (!edge_param || !*edge_param
        || (*edge_param)->type != ElementEnum::GeometryEdge
        || (*edge_param)->ids.size() < 2)
        return std::string("请选择两条或多条需要合并的几何边。");

    try {
        std::optional<Index> component_id;
        std::vector<TopoDS_Edge> edges;
        edges.reserve((*edge_param)->ids.size());

        // 从每个全局边 ID 反查组件，选择集必须完全属于同一个组件。
        for (Index edge_id : (*edge_param)->ids) {
            const auto edge_component_id =
                ctx.model.findComponentIdByGeometryShapeId(TopAbs_EDGE, edge_id);
            if (!edge_component_id)
                return std::string("所选几何边不属于任何组件。");
            if (component_id && *edge_component_id != *component_id)
                return std::string("待合并边必须属于同一个组件。");
            component_id = *edge_component_id;

            const TopoDS_Shape* edge_shape = ctx.model.geomRegistry().getEdge(edge_id);
            if (!edge_shape || edge_shape->ShapeType() != TopAbs_EDGE)
                return std::string("所选几何边已失效。");
            edges.push_back(TopoDS::Edge(*edge_shape));
        }

        ComponentData* component = ctx.model.findComponent(*component_id);
        if (!component || !component->geometry || !component->geometry->rootShape)
            return std::string("目标组件没有几何。");
        if (component->mapping && !component->mapping->empty())
            return std::string("目标组件已经建立几何-网格映射，不能修改几何拓扑。");

        // 原子操作成功后只替换一次完整根形状，使框架生成一条 undo 记录并重建子形状 ID。
        TopoDS_Shape result = GeometryTopologyEditor::mergeEdges(
            *component->geometry->rootShape, edges);
        auto component_operator = ctx.componentOperator(*component_id);
        if (!component_operator)
            return std::string("几何操作失败，详细原因请查看日志。");
        component_operator->replaceGeometryRoot(std::move(result));
        return std::string("合并边成功");
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("MergeEdge: {}", detail ? detail : "OpenCASCADE error");
    } catch (const std::exception& error) {
        spdlog::error("MergeEdge: {}", error.what());
    }
    return std::string("几何边合并失败，请确认所选边首尾相接且位于同一曲线；详细原因请查看日志。");
}
}
