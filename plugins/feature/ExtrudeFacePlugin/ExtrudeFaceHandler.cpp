#include "ExtrudeFaceHandler.h"
#include "ComponentData.h"
#include "FeatureContext.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "GeometryBuilder.h"
#include "GeometryData.h"

#include <BRepBuilderAPI_Copy.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <algorithm>
#include <spdlog/spdlog.h>
#include <stdexcept>

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

void ExtrudeFaceHandler::setup(FeatureRegistrar& reg, FeatureContext& /*ctx*/)
{
    // 参数声明与原 GeometryOperationActions.qml 中 extrudeFaceInfo 保持一致；
    // 菜单与图标复用原"几何"页"拉伸面"按钮的声明
    reg.addParameter({ ArgTypeEnum::Selector, "截面", "GeometryFace", "请选择一个几何面" });
    reg.addParameter({ ArgTypeEnum::Float, "方向 X", "0", "方向不能为零向量" });
    reg.addParameter({ ArgTypeEnum::Float, "方向 Y", "0", "方向不能为零向量" });
    reg.addParameter({ ArgTypeEnum::Float, "方向 Z", "1", "方向不能为零向量" });
    reg.addParameter({ ArgTypeEnum::Float, "长度", "10", "必须大于几何容差" });
    reg.addMenuItem({ "几何", "拉伸面为实体", "qrc:/images/toolbar/Geometry/stretched_surface.svg" });
}

std::any ExtrudeFaceHandler::execute(FeatureContext& ctx)
{
    const auto* selection_param = ctx.params.value(0).get<ArgTypeEnum::Selector>();
    if (!selection_param || !*selection_param)
        return std::string("请选择一个几何面。");
    const std::shared_ptr<Selection>& selected = *selection_param;
    if (selected->type != ElementEnum::GeometryFace || selected->ids.size() != 1)
        return std::string("请选择一个几何面。");

    // 截面身份决定来源组件，不要求对象树预先选中该组件。
    const Index face_id = selected->ids.front();
    const auto component_id = selected->component_id >= 0
        ? std::optional<Index> { selected->component_id }
        : ctx.model.findComponentIdByGeometryShapeId(TopAbs_FACE, face_id);
    if (!component_id)
        return std::string("所选几何面已失效。");

    const double direction_x = floatParam(ctx, 1);
    const double direction_y = floatParam(ctx, 2);
    const double direction_z = floatParam(ctx, 3);
    const double length = floatParam(ctx, 4);

    try {
        ComponentData* component = ctx.model.findComponent(*component_id);
        if (!component || !component->geometry || !component->geometry->rootShape)
            return std::string("目标组件没有几何。");
        component->geometry->ensureIndexBuilt(ctx.model.geomRegistry());

        const auto& component_face_ids = component->geometry->index.face_local_to_global;
        if (std::find(component_face_ids.begin(), component_face_ids.end(), face_id)
            == component_face_ids.end())
            return std::string("所选几何面必须属于目标组件。");

        const TopoDS_Shape* source_shape = ctx.model.geomRegistry().getFace(face_id);
        if (!source_shape || source_shape->ShapeType() != TopAbs_FACE)
            return std::string("所选几何面已失效。");

        const TopoDS_Face source = TopoDS::Face(*source_shape);
        auto job = ctx.runWritebackJob(
            "拉伸面为实体", *component_id,
            [source](const ComponentOperator&) {
                // 几何隔离由插件负责；worker 不访问模型和渲染器共享的旧 TShape。
                return TopoDS::Face(BRepBuilderAPI_Copy(source, true, false).Shape()); }, [direction_x, direction_y, direction_z, length](TopoDS_Face& input, systems::job::ProgressFn report) {
                report(0.1, "构造拉伸实体并检查拓扑");
                try {
                    auto solid = GeometryBuilder::extrudeFace(input, direction_x, direction_y, direction_z, length);
                    report(1.0, "拉伸计算完成");
                    return solid;
                } catch (const Standard_Failure& error) {
                    const char* detail = error.GetMessageString();
                    throw std::runtime_error(detail ? detail : "OpenCASCADE extrusion failed");
                } },
            [](ComponentOperator& target, TopoDS_Shape& solid, systems::job::ProgressFn report) {
                target.appendGeometryShape(std::move(solid));
                report(1.0, "拉伸完成：已将实体追加到源组件。");
            },
            true);
        return std::string(job ? "正在计算拉伸…" : "无法启动拉伸任务（任务忙碌或宿主未配置后台执行器）");
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("ExtrudeFace: {}", detail ? detail : "OpenCASCADE error");
    } catch (const std::exception& error) {
        spdlog::error("ExtrudeFace: {}", error.what());
    }
    return std::string("几何操作失败，详细原因请查看日志。");
}
}
