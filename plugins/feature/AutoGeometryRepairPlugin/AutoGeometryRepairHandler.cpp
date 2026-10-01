#include "AutoGeometryRepairHandler.h"
#include "ComponentData.h"
#include "ComponentOperator.h"
#include "FeatureContext.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "GeometryData.h"
#include "GeometryTopologyEditor.h"
#include "ModelLayer.h"

#include <Standard_Failure.hxx>
#include <TopoDS_Shape.hxx>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace systems::feature {
namespace {
constexpr size_t kTargetParam = 0;
constexpr size_t kToleranceParam = 1;
constexpr size_t kModeParam = 2;
}

void AutoGeometryRepairHandler::setup(FeatureRegistrar& reg, FeatureContext& /*ctx*/)
{
    // 选择范围与执行范围一致：只处理显式选中的一个组件内的全部自由边。
    reg.addParameter({ ArgTypeEnum::Selector, "目标组件", "Component", "请选择一个几何组件，将检测或修复该组件内的全部自由边" });
    reg.addParameter({ ArgTypeEnum::Float, "全局清理容差", "0.01", "用于所选组件内的间隙检测和自动缝合，使用模型长度单位" });
    reg.addParameter({ ArgTypeEnum::Combo, "执行模式", "仅检测,检测并修复|0", "仅检测不会修改几何" });
    reg.addMenuItem({ "几何/修复", "自动修复间隙", "" });
}

std::any AutoGeometryRepairHandler::execute(FeatureContext& ctx)
{
    const auto* target_param = ctx.params.value(kTargetParam).get<ArgTypeEnum::Selector>();
    if (!target_param || !*target_param
        || (*target_param)->type != ElementEnum::Component
        || (*target_param)->ids.size() != 1) {
        return std::string("请选择一个几何组件。");
    }
    const double* tolerance = ctx.params.value(kToleranceParam).get<ArgTypeEnum::Float>();
    if (!tolerance || !std::isfinite(*tolerance) || *tolerance <= 0.0)
        return std::string("全局清理容差必须大于零。");
    const int* mode = ctx.params.value(kModeParam).get<ArgTypeEnum::Combo>();
    if (!mode || *mode < 0 || *mode > 1)
        return std::string("执行模式参数无效。");

    try {
        const Index component_id = (*target_param)->ids.front();
        ComponentData* component = ctx.model.findComponent(component_id);
        if (!component)
            return std::string("所选组件已失效。");
        if (!component->geometry || !component->geometry->rootShape)
            return std::string("目标组件没有几何。");
        if (component->mapping && !component->mapping->empty())
            return std::string("目标组件已经建立几何-网格映射，不能修改几何拓扑。");

        ctx.model.setGeometryCleanupTolerance(*tolerance);
        const double cleanup_tolerance = ctx.model.geometryCleanupTolerance();
        const TopoDS_Shape& root = *component->geometry->rootShape;
        const std::vector<GeometryStitchCandidate> candidates =
            GeometryTopologyEditor::findStitchCandidates(root, cleanup_tolerance);
        if (*mode == 0) {
            double maximum_gap = 0.0;
            for (const GeometryStitchCandidate& candidate : candidates)
                maximum_gap = std::max(maximum_gap, candidate.maximum_gap);
            std::ostringstream message;
            message << "检测到 " << candidates.size() << " 对可 Stitch 自由边";
            if (!candidates.empty())
                message << "，最大间隙 " << std::setprecision(6) << maximum_gap;
            return message.str();
        }
        if (candidates.empty())
            return std::string("未找到全局清理容差内可修复的跨面自由边。");

        GeometryGapRepairResult repair =
            GeometryTopologyEditor::repairFreeEdgeGaps(root, cleanup_tolerance);
        auto component_operator = ctx.componentOperator
            ? ctx.componentOperator(component_id)
            : std::nullopt;
        if (!component_operator)
            return std::string("几何操作失败，详细原因请查看日志。");
        spdlog::info("AutoGeometryRepair: {} candidates, {} stitched edges",
            repair.candidate_count, repair.stitched_edge_count);
        component_operator->replaceGeometryRoot(std::move(repair.shape));
        return std::string("自动修复间隙成功，候选 ") + std::to_string(repair.candidate_count)
            + " 对，已缝合 " + std::to_string(repair.stitched_edge_count) + " 条边";
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("AutoGeometryRepair: {}", detail ? detail : "OpenCASCADE error");
    } catch (const std::exception& error) {
        spdlog::error("AutoGeometryRepair: {}", error.what());
    }
    return std::string("自动修复间隙失败，详细原因请查看日志。");
}
}
