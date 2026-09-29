#include "SplitEdgeHandler.h"
#include "ComponentData.h"
#include "ComponentOperator.h"
#include "FeatureContext.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "GeometryData.h"
#include "GeometryTopologyEditor.h"
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <spdlog/spdlog.h>
#include <exception>
#include <optional>
#include <string>
#include <utility>

namespace systems::feature {
void SplitEdgeHandler::setup(FeatureRegistrar& reg, FeatureContext& /*ctx*/)
{
    reg.addParameter({ ArgTypeEnum::Selector, "目标边", "GeometryEdge", "请选择一条需要分割的几何边" });
    reg.addParameter({ ArgTypeEnum::Float, "分割比例", "0.5", "沿边方向的归一化比例，范围为 (0, 1)" });
    reg.addMenuItem({ "几何/拓扑", "分割边", "" });
}

std::any SplitEdgeHandler::execute(FeatureContext& ctx)
{
    const auto* selection = ctx.params.value(0).get<ArgTypeEnum::Selector>();
    const auto* ratio = ctx.params.value(1).get<ArgTypeEnum::Float>();
    if (!selection || !*selection || (*selection)->type != ElementEnum::GeometryEdge
        || (*selection)->ids.size() != 1)
        return std::string("请选择一条需要分割的几何边。");
    if (!ratio || *ratio <= 0.0 || *ratio >= 1.0)
        return std::string("分割比例必须位于 0 和 1 之间。");
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
        TopoDS_Shape result = GeometryTopologyEditor::splitEdge(
            *component->geometry->rootShape, TopoDS::Edge(*shape), *ratio);
        auto op = ctx.componentOperator ? ctx.componentOperator(*component_id) : std::nullopt;
        return op ? std::any(op->replaceGeometryRoot(std::move(result)))
                  : std::any(std::string("几何操作失败，详细原因请查看日志。"));
    } catch (const std::exception& error) {
        spdlog::error("SplitEdge: {}", error.what());
        return std::string("几何边分割失败，详细原因请查看日志。");
    }
}
}
