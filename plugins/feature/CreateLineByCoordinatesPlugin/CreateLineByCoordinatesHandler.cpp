#include "CreateLineByCoordinatesHandler.h"
#include "FeatureContext.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "GeometryBuilder.h"
#include "GeometryShapeWriter.h"

#include <Standard_Failure.hxx>
#include <TopoDS_Shape.hxx>
#include <spdlog/spdlog.h>

namespace systems::feature {
namespace {
    /**
     * @brief 读取 Float 参数当前值（类型不符时回落默认值）
     */
    double floatParam(FeatureContext& ctx, std::size_t index, double fallback = 0.0)
    {
        if (const auto* value = ctx.params.value(index).get<ArgTypeEnum::Float>())
            return *value;
        return fallback;
    }
}

void CreateLineByCoordinatesHandler::setup(FeatureRegistrar& reg, FeatureContext& /*ctx*/)
{
    // 参数声明与原 GeometryOperationActions.qml 中 createLineByCoordinatesInfo 保持一致；
    // 菜单与图标复用原"几何"页"直线边（坐标）"按钮的声明
    reg.addParameter({ ArgTypeEnum::Float, "起点 X", "0", "" });
    reg.addParameter({ ArgTypeEnum::Float, "起点 Y", "0", "" });
    reg.addParameter({ ArgTypeEnum::Float, "起点 Z", "0", "" });
    reg.addParameter({ ArgTypeEnum::Float, "终点 X", "10", "" });
    reg.addParameter({ ArgTypeEnum::Float, "终点 Y", "0", "" });
    reg.addParameter({ ArgTypeEnum::Float, "终点 Z", "0", "" });
    reg.addParameter({ ArgTypeEnum::Combo, "写入目标",
        "添加到当前 Component,新建 Component,新建 Model|0", "选择几何创建结果的组织位置" });
    reg.addMenuItem({ "几何", "创建直线边（坐标）", "qrc:/images/toolbar/Geometry/line_coord.svg" });
}

std::any CreateLineByCoordinatesHandler::execute(FeatureContext& ctx)
{
    const double start_x = floatParam(ctx, 0);
    const double start_y = floatParam(ctx, 1);
    const double start_z = floatParam(ctx, 2);
    const double end_x = floatParam(ctx, 3);
    const double end_y = floatParam(ctx, 4);
    const double end_z = floatParam(ctx, 5);
    const auto* target_param = ctx.params.value(6).get<ArgTypeEnum::Combo>();
    const int write_target = target_param ? static_cast<int>(*target_param) : -1;

    // 活动模型/组件是对象树选中态提示，仅作缺省目标；提示语与原几何界面一致
    const auto active_model = ctx.activeModel ? ctx.activeModel() : std::nullopt;
    const auto active_component = ctx.activeComponent ? ctx.activeComponent() : std::nullopt;
    GeometryShapeWriter::WriteTarget target;
    if (write_target == 0) {
        if (!active_component)
            return std::string("请选择当前 Component，或修改写入目标。");
        target = { active_model.value_or(-1), *active_component };
    } else if (write_target == 1) {
        if (!active_model)
            return std::string("请选择当前 Model，或将写入目标改为“新建 Model”。");
        target = { *active_model, -1 };
    } else if (write_target == 2) {
        target = { -1, -1 };
    } else {
        return std::string("写入目标参数无效。");
    }

    try {
        TopoDS_Shape shape = GeometryBuilder::makeLine(
            start_x, start_y, start_z, end_x, end_y, end_z);
        const Index component_id = GeometryShapeWriter::writeShape(
            ctx.model, target, "Line_" + std::to_string(next_line_number_), std::move(shape));
        if (target.component_id < 0)
            ++next_line_number_;
        return component_id;
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("CreateLineByCoordinates: {}", detail ? detail : "OpenCASCADE error");
    } catch (const std::exception& error) {
        spdlog::error("CreateLineByCoordinates: {}", error.what());
    }
    return std::string("几何操作失败，详细原因请查看日志。");
}
}
