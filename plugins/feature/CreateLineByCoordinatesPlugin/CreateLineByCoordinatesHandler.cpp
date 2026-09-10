#include "CreateLineByCoordinatesHandler.h"
#include "FeatureContext.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "GeometryBuilder.h"
#include "ComponentData.h"
#include "GeometryData.h"
#include "ModelLayer.h"
#include "ModelOperator.h"

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
    Index target_model_id = -1;
    Index target_component_id = -1;
    if (write_target == 0) {
        if (!active_component)
            return std::string("请选择当前 Component，或修改写入目标。");
        target_model_id = active_model.value_or(-1);
        target_component_id = *active_component;
    } else if (write_target == 1) {
        if (!active_model)
            return std::string("请选择当前 Model，或将写入目标改为“新建 Model”。");
        target_model_id = *active_model;
    } else if (write_target == 2) {
    } else {
        return std::string("写入目标参数无效。");
    }

    try {
        TopoDS_Shape shape = GeometryBuilder::makeLine(
            start_x, start_y, start_z, end_x, end_y, end_z);
        // 组件目标优先：追加到既有组件几何（写入即标脏，通知由操作边界 flush）
        if (target_component_id >= 0) {
            auto component_operator = ctx.model.getComponentOperator(target_component_id);
            if (!component_operator)
                return std::string("几何操作失败，详细原因请查看日志。");
            return component_operator->appendGeometryShape(std::move(shape));
        }

        // 新建几何组件；无目标模型时先经 addModel 新建临时模型承载
        if (target_model_id < 0)
            target_model_id = ctx.model.addModel("temp_Line_" + std::to_string(next_line_number_), {});
        auto geometry = std::make_unique<GeometryData>();
        geometry->setRootShape(std::move(shape));
        auto component = std::make_unique<ComponentData>();
        component->name = "Line_" + std::to_string(next_line_number_);
        component->geometry = std::move(geometry);
        auto model_operator = ctx.model.getModelOperator(target_model_id);
        if (!model_operator)
            return std::string("几何操作失败，详细原因请查看日志。");
        const Index component_id = model_operator->addGeometryComponent(std::move(component));
        ++next_line_number_; // 新建组件，推进编号
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
