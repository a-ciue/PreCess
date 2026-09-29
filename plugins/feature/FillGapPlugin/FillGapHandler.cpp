#include "FillGapHandler.h"
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

#include <cmath>
#include <exception>
#include <optional>
#include <string>
#include <utility>

namespace systems::feature {
namespace {
constexpr size_t kSeedEdgeParam = 0;
constexpr size_t kToleranceParam = 1;
constexpr size_t kModeParam = 2;
}

void FillGapHandler::setup(FeatureRegistrar& reg, FeatureContext& /*ctx*/)
{
    reg.addParameter({ ArgTypeEnum::Selector, "间隙边", "GeometryEdge",
        "请选择一条间隙边界上的自由边" });
    reg.addParameter({ ArgTypeEnum::Float, "边界识别容差", "0.01",
        "搜索对侧边界的最大距离；局部缝合时也用作缝合容差，使用模型长度单位" });
    reg.addParameter({ ArgTypeEnum::Combo, "操作方式", "局部缝合,补面|0",
        "局部缝合连接已有面；补面自动封闭孔洞或桥接两侧边界，支持非共面边界" });
    reg.addMenuItem({ "几何/修复", "局部缝合", "" });
}

std::any FillGapHandler::execute(FeatureContext& ctx)
{
    const auto* seed_param = ctx.params.value(kSeedEdgeParam).get<ArgTypeEnum::Selector>();
    if (!seed_param || !*seed_param || (*seed_param)->type != ElementEnum::GeometryEdge
        || (*seed_param)->ids.size() != 1)
        return std::string("请选择一条间隙边界上的几何边。");

    const double* tolerance = ctx.params.value(kToleranceParam).get<ArgTypeEnum::Float>();
    if (!tolerance || !std::isfinite(*tolerance) || *tolerance <= 0.0)
        return std::string("边界识别容差必须大于零。");
    const int* mode = ctx.params.value(kModeParam).get<ArgTypeEnum::Combo>();
    if (!mode || *mode < 0 || *mode > 1)
        return std::string("请选择局部缝合或补面。");

    try {
        const Index edge_id = (*seed_param)->ids.front();
        const auto component_id =
            ctx.model.findComponentIdByGeometryShapeId(TopAbs_EDGE, edge_id);
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

        ctx.model.setGeometryCleanupTolerance(*tolerance);
        const double cleanup_tolerance = ctx.model.geometryCleanupTolerance();
        // 两种操作均先生成并校验完整结果，再一次写回，保持一次执行对应一次撤销。
        TopoDS_Shape result = *mode == 0
            ? GeometryTopologyEditor::stitchGapFromSeedEdge(
                  *component->geometry->rootShape, TopoDS::Edge(*edge_shape), cleanup_tolerance)
            : GeometryTopologyEditor::fillGapFromSeedEdge(
                  *component->geometry->rootShape, TopoDS::Edge(*edge_shape), cleanup_tolerance);

        auto component_operator = ctx.componentOperator
            ? ctx.componentOperator(*component_id)
            : std::nullopt;
        if (!component_operator)
            return std::string("几何操作失败，详细原因请查看日志。");
        return component_operator->replaceGeometryRoot(std::move(result));
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("FillGap: {}", detail ? detail : "OpenCASCADE error");
    } catch (const std::exception& error) {
        spdlog::error("FillGap: {}", error.what());
    }
    return *mode == 0
        ? std::string("局部缝合失败，请确认所选边是容差内间隙边界上的自由边；详细原因请查看日志。")
        : std::string("补面失败：需要无分支的闭合自由边界或容差内的对侧边链，且新面不能覆盖已有面；详细原因请查看日志。");
}
}
