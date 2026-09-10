#include "ExtrudeFaceHandler.h"
#include "ComponentData.h"
#include "GeometryData.h"
#include "FeatureContext.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "GeometryBuilder.h"
#include "GeometryShapeWriter.h"

#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <algorithm>
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

void ExtrudeFaceHandler::setup(FeatureRegistrar& reg, FeatureContext& /*ctx*/)
{
    // 参数声明与原 GeometryOperationActions.qml 中 extrudeFaceInfo 保持一致；
    // 菜单与图标复用原"几何"页"拉伸面"按钮的声明
    reg.addParameter({ ArgTypeEnum::Selector, "截面", "", "请选择一个几何面" });
    reg.addParameter({ ArgTypeEnum::Float, "方向 X", "0", "方向不能为零向量" });
    reg.addParameter({ ArgTypeEnum::Float, "方向 Y", "0", "方向不能为零向量" });
    reg.addParameter({ ArgTypeEnum::Float, "方向 Z", "1", "方向不能为零向量" });
    reg.addParameter({ ArgTypeEnum::Float, "长度", "10", "必须大于几何容差" });
    reg.addMenuItem({ "几何", "拉伸面为实体", "qrc:/images/toolbar/Geometry/stretched_surface.svg" });
}

std::any ExtrudeFaceHandler::execute(FeatureContext& ctx)
{
    // 依赖已有拓扑的操作写回其来源 Component（对象树选中态提示）
    const auto component_id = ctx.activeComponent ? ctx.activeComponent() : std::nullopt;
    if (!component_id)
        return std::string("请先选择目标 Component。");

    const auto* selection_param = ctx.params.value(0).get<ArgTypeEnum::Selector>();
    if (!selection_param || !*selection_param)
        return std::string("请选择一个几何面。");
    const std::shared_ptr<Selection>& selected = *selection_param;
    if (selected->type != ElementEnum::GeometryFace || selected->ids.size() != 1)
        return std::string("请选择一个几何面。");

    const double direction_x = floatParam(ctx, 1);
    const double direction_y = floatParam(ctx, 2);
    const double direction_z = floatParam(ctx, 3);
    const double length = floatParam(ctx, 4);

    try {
        ComponentData* component = ctx.model.findComponent(*component_id);
        if (!component || !component->geometry || !component->geometry->rootShape)
            return std::string("目标组件没有几何。");
        component->geometry->ensureIndexBuilt(ctx.model.geomRegistry());

        const Index face_id = selected->ids.front();
        const auto& component_face_ids = component->geometry->index.face_local_to_global;
        if (std::find(component_face_ids.begin(), component_face_ids.end(), face_id)
            == component_face_ids.end())
            return std::string("所选几何面必须属于目标组件。");

        const TopoDS_Shape* source_shape = ctx.model.geomRegistry().getFace(face_id);
        if (!source_shape || source_shape->ShapeType() != TopAbs_FACE)
            return std::string("所选几何面已失效。");

        // 在追加结果导致索引重建前复制句柄，构造器内部再复制源 Face 的拓扑。
        const TopoDS_Face source = TopoDS::Face(*source_shape);
        TopoDS_Shape solid = GeometryBuilder::extrudeFace(
            source, direction_x, direction_y, direction_z, length);
        return GeometryShapeWriter::writeShape(ctx.model,
            GeometryShapeWriter::WriteTarget { -1, *component_id },
            "Extrude", std::move(solid));
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("ExtrudeFace: {}", detail ? detail : "OpenCASCADE error");
    } catch (const std::exception& error) {
        spdlog::error("ExtrudeFace: {}", error.what());
    }
    return std::string("几何操作失败，详细原因请查看日志。");
}
}
