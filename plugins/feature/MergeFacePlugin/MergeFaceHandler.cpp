#include "MergeFaceHandler.h"
#include "ComponentData.h"
#include "ComponentOperator.h"
#include "FeatureContext.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "GeometryData.h"
#include "GeometryTopologyEditor.h"

#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <spdlog/spdlog.h>

#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace systems::feature {
void MergeFaceHandler::setup(FeatureRegistrar& reg, FeatureContext& /*ctx*/)
{
    reg.addParameter({ ArgTypeEnum::Selector, "待合并面", "GeometryFace", "请选择两个或多个连通且同域的几何面" });
    reg.navigation().addEntry({ "MergeFace", "合并面", "", 0, "几何/拓扑" });
}

std::any MergeFaceHandler::execute(FeatureContext& ctx)
{
    const auto* face_param = ctx.params.value(0).get<ArgTypeEnum::Selector>();
    if (!face_param || !*face_param
        || (*face_param)->type != ElementEnum::GeometryFace
        || (*face_param)->ids.size() < 2)
        return std::string("请选择两个或多个需要合并的几何面。");

    try {
        std::optional<Index> component_id;
        std::vector<TopoDS_Face> faces;
        faces.reserve((*face_param)->ids.size());

        // 从每个全局面 ID 反查组件，选择集必须完全属于同一个组件。
        for (Index face_id : (*face_param)->ids) {
            const auto face_component_id =
                ctx.model.findComponentIdByGeometryShapeId(TopAbs_FACE, face_id);
            if (!face_component_id)
                return std::string("所选几何面不属于任何组件。");
            if (component_id && *face_component_id != *component_id)
                return std::string("待合并面必须属于同一个组件。");
            component_id = *face_component_id;

            const TopoDS_Shape* face_shape = ctx.model.geomRegistry().getFace(face_id);
            if (!face_shape || face_shape->ShapeType() != TopAbs_FACE)
                return std::string("所选几何面已失效。");
            faces.push_back(TopoDS::Face(*face_shape));
        }

        ComponentData* component = ctx.model.findComponent(*component_id);
        if (!component || !component->geometry || !component->geometry->rootShape)
            return std::string("目标组件没有几何。");
        if (component->mapping && !component->mapping->empty())
            return std::string("目标组件已经建立几何-网格映射，不能修改几何拓扑。");

        // 原子操作成功后只替换一次完整根形状，使框架生成一条 undo 记录并重建子形状 ID。
        TopoDS_Shape result = GeometryTopologyEditor::mergeFaces(
            *component->geometry->rootShape, faces);
        auto component_operator = ctx.componentOperator(*component_id);
        if (!component_operator)
            return std::string("几何操作失败，详细原因请查看日志。");
        component_operator->replaceGeometryRoot(std::move(result));
        return std::string("合并面成功");
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("MergeFace: {}", detail ? detail : "OpenCASCADE error");
    } catch (const std::exception& error) {
        spdlog::error("MergeFace: {}", error.what());
    }
    return std::string("几何面合并失败，请确认所选面彼此连通且位于同一曲面；详细原因请查看日志。");
}
}
