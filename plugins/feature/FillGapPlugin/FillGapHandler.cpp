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
}

void FillGapHandler::setup(FeatureRegistrar& reg, FeatureContext& /*ctx*/)
{
    reg.addParameter({ ArgTypeEnum::Selector, "间隙边", "GeometryEdge",
        "请选择一条间隙边界上的自由边" });
    reg.addParameter({ ArgTypeEnum::Float, "最大间隙", "0.01",
        "允许识别和缝合的最大间隙；优先移动选中侧，双向失败后自动尝试局部容差缝合；使用模型长度单位" });
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
        return std::string("最大间隙必须大于零。");

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

        ctx.model.setGeometryCleanupTolerance(*tolerance);
        const double cleanup_tolerance = ctx.model.geometryCleanupTolerance();
        // 先按选中侧到对侧的方向重建几何，再一次写回，形成一条撤销记录。
        bool reversed = false;
        bool sewn = false;
        TopoDS_Shape result = GeometryTopologyEditor::stitchGapFromSeedEdge(
            *component->geometry->rootShape, TopoDS::Edge(*edge_shape), cleanup_tolerance, &reversed, &sewn);
        auto component_operator = ctx.componentOperator(*component_id);
        if (!component_operator)
            return std::string("几何操作失败，详细原因请查看日志。");
        component_operator->replaceGeometryRoot(std::move(result));
        if (sewn)
            return std::string("补间隙成功：已使用容差缝合，结果通过连接误差验收；未执行单侧定向移动。");
        if (reversed)
            return std::string("补间隙成功：已反向缝合，选中侧保持原位。");
        return std::string("补间隙成功");
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("FillGap: {}", detail ? detail : "OpenCASCADE error");
    } catch (const std::exception& error) {
        spdlog::error("FillGap: {}", error.what());
    }
    return std::string("局部缝合失败，无法匹配对侧边界或重建有效邻面；详细原因请查看日志。");
}
}