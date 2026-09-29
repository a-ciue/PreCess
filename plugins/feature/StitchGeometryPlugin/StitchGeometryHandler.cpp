#include "StitchGeometryHandler.h"
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
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Pnt.hxx>
#include <spdlog/spdlog.h>

#include <cmath>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace systems::feature {
namespace {
constexpr size_t kFirstSelectionParam = 0;
constexpr size_t kSecondSelectionParam = 1;
constexpr size_t kToleranceParam = 2;
constexpr size_t kVertexTargetParam = 3;
}

void StitchGeometryHandler::setup(FeatureRegistrar& reg, FeatureContext& /*ctx*/)
{
    reg.addParameter({ ArgTypeEnum::Selector, "第一组拓扑", "GeometryVertex,GeometryEdge",
        "请选择第一个点，或第一组连续自由边" });
    reg.addParameter({ ArgTypeEnum::Selector, "第二组拓扑", "GeometryVertex,GeometryEdge",
        "请选择第二个点，或第二组连续自由边" });
    reg.addParameter({ ArgTypeEnum::Float, "缝合容差", "0.01",
        "两组拓扑之间允许缝合的最大距离，使用当前模型长度单位" });
    reg.addParameter({ ArgTypeEnum::Combo, "点目标位置", "保留第一点,保留第二点,中点|0",
        "仅点缝合使用" });
    reg.addMenuItem({ "几何/修复", "缝合", "" });
}

std::any StitchGeometryHandler::execute(FeatureContext& ctx)
{
    const auto* first_param = ctx.params.value(kFirstSelectionParam).get<ArgTypeEnum::Selector>();
    const auto* second_param = ctx.params.value(kSecondSelectionParam).get<ArgTypeEnum::Selector>();
    if (!first_param || !*first_param || (*first_param)->ids.empty())
        return std::string("请选择第一个点，或第一组连续自由边。");
    if (!second_param || !*second_param || (*second_param)->ids.empty())
        return std::string("请选择第二个点，或第二组连续自由边。");
    if ((*first_param)->type != (*second_param)->type)
        return std::string("两组选择必须同时为几何点或同时为几何边。");

    const ElementEnum::Type selection_type = (*first_param)->type;
    if (selection_type != ElementEnum::GeometryVertex
        && selection_type != ElementEnum::GeometryEdge)
        return std::string("几何缝合只支持点或自由边界链。");
    if (selection_type == ElementEnum::GeometryVertex
        && ((*first_param)->ids.size() != 1 || (*second_param)->ids.size() != 1))
        return std::string("点缝合时每组必须各选择一个几何点。");

    const double* tolerance = ctx.params.value(kToleranceParam).get<ArgTypeEnum::Float>();
    if (!tolerance || !std::isfinite(*tolerance) || *tolerance <= 0.0)
        return std::string("缝合容差必须大于零。");

    try {
        std::optional<Index> component_id;
        const TopAbs_ShapeEnum shape_type = selection_type == ElementEnum::GeometryVertex
            ? TopAbs_VERTEX
            : TopAbs_EDGE;
        const auto verify_component = [&](const std::vector<Index>& shape_ids) {
            for (Index shape_id : shape_ids) {
                const auto owner = ctx.model.findComponentIdByGeometryShapeId(shape_type, shape_id);
                if (!owner)
                    return false;
                if (component_id && *owner != *component_id)
                    return false;
                component_id = *owner;
            }
            return true;
        };
        if (!verify_component((*first_param)->ids)
            || !verify_component((*second_param)->ids))
            return std::string("待缝合拓扑必须属于同一个组件。");

        ComponentData* component = ctx.model.findComponent(*component_id);
        if (!component || !component->geometry || !component->geometry->rootShape)
            return std::string("目标组件没有几何。");
        if (component->mapping && !component->mapping->empty())
            return std::string("目标组件已经建立几何-网格映射，不能修改几何拓扑。");

        const TopoDS_Shape& root = *component->geometry->rootShape;
        ctx.model.setGeometryCleanupTolerance(*tolerance);
        const double cleanup_tolerance = ctx.model.geometryCleanupTolerance();
        TopoDS_Shape result;
        if (selection_type == ElementEnum::GeometryVertex) {
            const TopoDS_Shape* first_shape
                = ctx.model.geomRegistry().getVertex((*first_param)->ids.front());
            const TopoDS_Shape* second_shape
                = ctx.model.geomRegistry().getVertex((*second_param)->ids.front());
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
            if (first_point.Distance(second_point) > cleanup_tolerance)
                return std::string("两个点的距离超过缝合容差。");

            const int* target_mode
                = ctx.params.value(kVertexTargetParam).get<ArgTypeEnum::Combo>();
            if (!target_mode || *target_mode < 0 || *target_mode > 2)
                return std::string("点目标位置参数无效。");
            gp_Pnt target_position = first_point;
            if (*target_mode == 1)
                target_position = second_point;
            else if (*target_mode == 2)
                target_position.SetXYZ((first_point.XYZ() + second_point.XYZ()) * 0.5);
            result = GeometryTopologyEditor::mergeVertices(
                root, { first, second }, target_position);
        } else {
            const auto collect_edges = [&](const std::vector<Index>& edge_ids) {
                std::vector<TopoDS_Edge> edges;
                edges.reserve(edge_ids.size());
                for (Index edge_id : edge_ids) {
                    const TopoDS_Shape* shape = ctx.model.geomRegistry().getEdge(edge_id);
                    if (!shape || shape->ShapeType() != TopAbs_EDGE)
                        throw std::invalid_argument("Selected geometry edge is no longer valid");
                    edges.push_back(TopoDS::Edge(*shape));
                }
                return edges;
            };
            result = GeometryTopologyEditor::stitchBoundaryEdges(root,
                collect_edges((*first_param)->ids), collect_edges((*second_param)->ids),
                cleanup_tolerance);
        }

        auto component_operator = ctx.componentOperator
            ? ctx.componentOperator(*component_id)
            : std::nullopt;
        if (!component_operator)
            return std::string("几何操作失败，详细原因请查看日志。");
        return component_operator->replaceGeometryRoot(std::move(result));
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("StitchGeometry: {}", detail ? detail : "OpenCASCADE error");
    } catch (const std::exception& error) {
        spdlog::error("StitchGeometry: {}", error.what());
    }
    return std::string("几何缝合失败，请确认选择的是容差内的两个点或两组连续自由边；详细原因请查看日志。");
}
}
