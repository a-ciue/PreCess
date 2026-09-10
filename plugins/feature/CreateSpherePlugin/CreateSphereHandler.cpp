#include "CreateSphereHandler.h"
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
#include <cmath>
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

    // 界面对用户统一使用度数，进入 GeometryBuilder 前转换为 OCC 使用的弧度
    double degreesToRadians(double angle_degrees)
    {
        return angle_degrees * std::acos(-1.0) / 180.0;
    }
}

void CreateSphereHandler::setup(FeatureRegistrar& reg, FeatureContext& /*ctx*/)
{
    // 参数声明与原 GeometryOperationActions.qml 中 createSphereInfo 保持一致；
    // 菜单与图标复用原"几何"页"球体/部分球体"按钮的声明
    reg.addParameter({ ArgTypeEnum::Float, "球心 X", "0", "" });
    reg.addParameter({ ArgTypeEnum::Float, "球心 Y", "0", "" });
    reg.addParameter({ ArgTypeEnum::Float, "球心 Z", "0", "" });
    reg.addParameter({ ArgTypeEnum::Float, "半径", "10", "必须大于几何容差" });
    reg.addParameter({ ArgTypeEnum::Float, "极轴 X", "0", "极轴不能为零向量" });
    reg.addParameter({ ArgTypeEnum::Float, "极轴 Y", "0", "极轴不能为零向量" });
    reg.addParameter({ ArgTypeEnum::Float, "极轴 Z", "1", "极轴不能为零向量" });
    reg.addParameter({ ArgTypeEnum::Float, "最小纬度（度）", "-90", "范围为 [-90, 90)，且小于最大纬度" });
    reg.addParameter({ ArgTypeEnum::Float, "最大纬度（度）", "90", "范围为 (-90, 90]，且大于最小纬度" });
    reg.addParameter({ ArgTypeEnum::Float, "经度扫掠角（度）", "360", "范围为 (0, 360]" });
    reg.addParameter({ ArgTypeEnum::Combo, "写入目标",
        "添加到当前 Component,新建 Component,新建 Model|0", "选择几何创建结果的组织位置" });
    reg.addMenuItem({ "几何", "创建球体/部分球体", "qrc:/images/toolbar/Geometry/sphere.svg" });
}

std::any CreateSphereHandler::execute(FeatureContext& ctx)
{
    const double center_x = floatParam(ctx, 0);
    const double center_y = floatParam(ctx, 1);
    const double center_z = floatParam(ctx, 2);
    const double radius = floatParam(ctx, 3);
    const double direction_x = floatParam(ctx, 4);
    const double direction_y = floatParam(ctx, 5);
    const double direction_z = floatParam(ctx, 6);
    const double minimum_latitude = floatParam(ctx, 7);
    const double maximum_latitude = floatParam(ctx, 8);
    const double longitude_sweep = floatParam(ctx, 9);
    const auto* target_param = ctx.params.value(10).get<ArgTypeEnum::Combo>();
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
        TopoDS_Shape shape = GeometryBuilder::makeSphere(center_x, center_y, center_z,
            radius, direction_x, direction_y, direction_z,
            degreesToRadians(minimum_latitude), degreesToRadians(maximum_latitude),
            degreesToRadians(longitude_sweep));
        // 组件目标优先：追加到既有组件几何（写入即标脏，通知由操作边界 flush）
        if (target_component_id >= 0) {
            auto component_operator = ctx.model.getComponentOperator(target_component_id);
            if (!component_operator)
                return std::string("几何操作失败，详细原因请查看日志。");
            return component_operator->appendGeometryShape(std::move(shape));
        }

        // 新建几何组件；无目标模型时先经 addModel 新建临时模型承载
        if (target_model_id < 0)
            target_model_id = ctx.model.addModel("temp_Sphere_" + std::to_string(next_sphere_number_), {});
        auto geometry = std::make_unique<GeometryData>();
        geometry->setRootShape(std::move(shape));
        auto component = std::make_unique<ComponentData>();
        component->name = "Sphere_" + std::to_string(next_sphere_number_);
        component->geometry = std::move(geometry);
        auto model_operator = ctx.model.getModelOperator(target_model_id);
        if (!model_operator)
            return std::string("几何操作失败，详细原因请查看日志。");
        const Index component_id = model_operator->addGeometryComponent(std::move(component));
        ++next_sphere_number_; // 新建组件，推进编号
        return component_id;
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("CreateSphere: {}", detail ? detail : "OpenCASCADE error");
    } catch (const std::exception& error) {
        spdlog::error("CreateSphere: {}", error.what());
    }
    return std::string("几何操作失败，详细原因请查看日志。");
}
}
