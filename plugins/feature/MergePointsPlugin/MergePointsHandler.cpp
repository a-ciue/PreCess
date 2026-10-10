#include "MergePointsHandler.h"
#include "ComponentData.h"
#include "ComponentOperator.h"
#include "FeatureContext.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "GeometryData.h"
#include "GeometryTopologyEditor.h"
#include "ModelLayer.h"

#include <BRep_Tool.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Pnt.hxx>
#include <spdlog/spdlog.h>

#include <exception>
#include <optional>
#include <string>
#include <utility>

namespace systems::feature {
namespace {
constexpr size_t kFirstPointParam = 0;
constexpr size_t kSecondPointParam = 1;
constexpr size_t kTargetParam = 2;
}

void MergePointsHandler::setup(FeatureRegistrar& reg, FeatureContext& /*ctx*/)
{
    reg.addParameter({ ArgTypeEnum::Selector, "第一个几何点", "GeometryVertex", "请选择第一个几何点" });
    reg.addParameter({ ArgTypeEnum::Selector, "第二个几何点", "GeometryVertex", "请选择第二个几何点" });
    reg.addParameter({ ArgTypeEnum::Combo, "合并到", "保留第一点,保留第二点,中点|0",
        "合并后公共顶点所在位置；相邻曲线和面随之变形" });
    reg.navigation().addEntry({ "MergePoints", "点合并", "", 0, "几何/拓扑" });
}

std::any MergePointsHandler::execute(FeatureContext& ctx)
{
    const auto* first_param = ctx.params.value(kFirstPointParam).get<ArgTypeEnum::Selector>();
    const auto* second_param = ctx.params.value(kSecondPointParam).get<ArgTypeEnum::Selector>();
    if (!first_param || !*first_param || (*first_param)->type != ElementEnum::GeometryVertex
        || (*first_param)->ids.size() != 1)
        return std::string("请选择第一个几何点。");
    if (!second_param || !*second_param || (*second_param)->type != ElementEnum::GeometryVertex
        || (*second_param)->ids.size() != 1)
        return std::string("请选择第二个几何点。");

    const int* target_mode = ctx.params.value(kTargetParam).get<ArgTypeEnum::Combo>();
    if (!target_mode || *target_mode < 0 || *target_mode > 2)
        return std::string("合并目标位置参数无效。");

    try {
        const Index first_id = (*first_param)->ids.front();
        const Index second_id = (*second_param)->ids.front();
        const auto first_owner =
            ctx.model.findComponentIdByGeometryShapeId(TopAbs_VERTEX, first_id);
        const auto second_owner =
            ctx.model.findComponentIdByGeometryShapeId(TopAbs_VERTEX, second_id);
        if (!first_owner || !second_owner)
            return std::string("所选几何点不属于任何组件。");
        if (*first_owner != *second_owner)
            return std::string("两个几何点必须属于同一个组件。");

        ComponentData* component = ctx.model.findComponent(*first_owner);
        if (!component || !component->geometry || !component->geometry->rootShape)
            return std::string("目标组件没有几何。");
        if (component->mapping && !component->mapping->empty())
            return std::string("目标组件已经建立几何-网格映射，不能修改几何拓扑。");

        const TopoDS_Shape* first_shape = ctx.model.geomRegistry().getVertex(first_id);
        const TopoDS_Shape* second_shape = ctx.model.geomRegistry().getVertex(second_id);
        if (!first_shape || !second_shape
            || first_shape->ShapeType() != TopAbs_VERTEX
            || second_shape->ShapeType() != TopAbs_VERTEX)
            return std::string("所选几何点已失效。");

        const TopoDS_Vertex first = TopoDS::Vertex(*first_shape);
        const TopoDS_Vertex second = TopoDS::Vertex(*second_shape);
        if (first.IsSame(second))
            return std::string("请选择两个不同的几何点。");

        const gp_Pnt first_point = BRep_Tool::Pnt(first);
        const gp_Pnt second_point = BRep_Tool::Pnt(second);

        gp_Pnt target_position = first_point;
        if (*target_mode == 1)
            target_position = second_point;
        else if (*target_mode == 2)
            target_position.SetXYZ((first_point.XYZ() + second_point.XYZ()) * 0.5);

        TopoDS_Shape result = GeometryTopologyEditor::mergeVertices(
            *component->geometry->rootShape, { first, second }, target_position);
        auto component_operator = ctx.componentOperator(*first_owner);
        if (!component_operator)
            return std::string("几何操作失败，详细原因请查看日志。");
        component_operator->replaceGeometryRoot(std::move(result));
        return std::string("点合并成功");
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("MergePoints: {}", detail ? detail : "OpenCASCADE error");
    } catch (const std::exception& error) {
        spdlog::error("MergePoints: {}", error.what());
    }
    return std::string("点合并失败，无法重建有效的相邻曲线或面；详细原因请查看日志。");
}
}
