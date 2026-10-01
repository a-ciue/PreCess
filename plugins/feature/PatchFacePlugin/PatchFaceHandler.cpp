#include "PatchFaceHandler.h"
#include "ComponentData.h"
#include "ComponentOperator.h"
#include "FeatureContext.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "GeometryData.h"
#include "GeometryTopologyEditor.h"
#include "ModelLayer.h"

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

namespace systems::feature {
namespace {
    constexpr size_t kSeedEdgeParam = 0;
}

void PatchFaceHandler::setup(FeatureRegistrar& reg, FeatureContext& /*ctx*/)
{
    reg.addParameter({ ArgTypeEnum::Selector, "边界边", "GeometryEdge",
        "选择一条孤立边或单面边界边，按总弧长最短的闭合边界环补面" });
    reg.addMenuItem({ "几何/修复", "补面", "" });
}

std::any PatchFaceHandler::execute(FeatureContext& ctx)
{
    const auto* seed_param = ctx.params.value(kSeedEdgeParam).get<ArgTypeEnum::Selector>();
    if (!seed_param || !*seed_param || (*seed_param)->type != ElementEnum::GeometryEdge
        || (*seed_param)->ids.size() != 1)
        return std::string("请选择一条待补孔洞上的几何边。");

    try {
        const Index edge_id = (*seed_param)->ids.front();
        const auto component_id = ctx.model.findComponentIdByGeometryShapeId(TopAbs_EDGE, edge_id);
        if (!component_id)
            return std::string("所选几何边不属于任何组件。");

        ComponentData* component = ctx.model.findComponent(*component_id);
        if (!component || !component->geometry || !component->geometry->rootShape)
            return std::string("目标组件没有几何。");
        if (component->mapping && !component->mapping->empty())
            return std::string("目标组件已经建立几何-网格映射，不能修改几何拓扑。");

        const TopoDS_Shape* edge_shape = ctx.model.geomRegistry().getEdge(edge_id);
        if (!edge_shape || edge_shape->ShapeType() != TopAbs_EDGE)
            return std::string("所选几何边已失效。");

        // 最小闭环由拓扑连通和弧长决定，不添加桥接边，也不依赖用户容差。
        TopoDS_Shape result = GeometryTopologyEditor::fillBoundaryLoop(
            *component->geometry->rootShape, TopoDS::Edge(*edge_shape));
        auto component_operator = ctx.componentOperator
            ? ctx.componentOperator(*component_id)
            : std::nullopt;
        if (!component_operator)
            return std::string("几何操作失败，详细原因请查看日志。");
        component_operator->replaceGeometryRoot(std::move(result));
        return std::string("补面成功");
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("PatchFace: {}", detail ? detail : "OpenCASCADE error");
    } catch (const std::exception& error) {
        spdlog::error("PatchFace: {}", error.what());
    }
    return std::string("补面失败，请确认所选边属于闭合孔洞边界，且最小环不会覆盖已有面；详细原因请查看日志。");
}
}