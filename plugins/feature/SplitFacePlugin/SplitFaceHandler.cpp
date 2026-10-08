#include "SplitFaceHandler.h"
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
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <spdlog/spdlog.h>

#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace systems::feature {
void SplitFaceHandler::setup(FeatureRegistrar& reg, FeatureContext& /*ctx*/)
{
    // 切割工具允许 Edge 或 Face，execute 按实际选择类型调用对应原子操作。
    reg.addParameter({ ArgTypeEnum::Selector, "目标面", "GeometryFace", "请选择一个需要分割的几何面" });
    reg.addParameter({ ArgTypeEnum::Selector, "切割工具", "GeometryEdge,GeometryFace", "请选择位于目标面上的边，或与目标面相交的面" });
    reg.addMenuItem({ "几何/拓扑", "分割面", "" });
}

std::any SplitFaceHandler::execute(FeatureContext& ctx)
{
    const auto* face_param = ctx.params.value(0).get<ArgTypeEnum::Selector>();
    if (!face_param || !*face_param
        || (*face_param)->type != ElementEnum::GeometryFace
        || (*face_param)->ids.size() != 1)
        return std::string("请选择一个需要分割的几何面。");

    const auto* tool_param = ctx.params.value(1).get<ArgTypeEnum::Selector>();
    if (!tool_param || !*tool_param || (*tool_param)->ids.empty()
        || ((*tool_param)->type != ElementEnum::GeometryEdge
            && (*tool_param)->type != ElementEnum::GeometryFace)) {
        return std::string("请选择用于分割目标面的几何边或几何面。");
    }

    try {
        const Index face_id = (*face_param)->ids.front();
        const TopoDS_Shape* face_shape = ctx.model.geomRegistry().getFace(face_id);
        if (!face_shape || face_shape->ShapeType() != TopAbs_FACE)
            return std::string("所选目标面已失效。");

        // 目标组件由几何面反查，避免依赖对象树当前选中态。
        const auto component_id = ctx.model.findComponentIdByGeometryShapeId(TopAbs_FACE, face_id);
        if (!component_id)
            return std::string("所选目标面不属于任何组件。");

        ComponentData* component = ctx.model.findComponent(*component_id);
        if (!component || !component->geometry || !component->geometry->rootShape)
            return std::string("目标组件没有几何。");
        if (component->mapping && !component->mapping->empty())
            return std::string("目标组件已经建立几何-网格映射，不能修改几何拓扑。");

        // 索引释放前复制轻量 OCC 句柄；所有切割工具必须与目标面属于同一组件。
        const TopoDS_Face target_face = TopoDS::Face(*face_shape);
        TopoDS_Shape result;
        if ((*tool_param)->type == ElementEnum::GeometryEdge) {
            std::vector<TopoDS_Edge> splitting_edges;
            splitting_edges.reserve((*tool_param)->ids.size());
            for (Index edge_id : (*tool_param)->ids) {
                const auto edge_component_id =
                    ctx.model.findComponentIdByGeometryShapeId(TopAbs_EDGE, edge_id);
                if (!edge_component_id || *edge_component_id != *component_id)
                    return std::string("目标面和切割工具必须属于同一个组件。");

                const TopoDS_Shape* edge_shape = ctx.model.geomRegistry().getEdge(edge_id);
                if (!edge_shape || edge_shape->ShapeType() != TopAbs_EDGE)
                    return std::string("所选分割边已失效。");
                splitting_edges.push_back(TopoDS::Edge(*edge_shape));
            }
            result = GeometryTopologyEditor::splitFace(
                *component->geometry->rootShape, target_face, splitting_edges);
        } else {
            std::vector<TopoDS_Face> splitting_faces;
            splitting_faces.reserve((*tool_param)->ids.size());
            for (Index tool_face_id : (*tool_param)->ids) {
                const auto tool_component_id =
                    ctx.model.findComponentIdByGeometryShapeId(TopAbs_FACE, tool_face_id);
                if (!tool_component_id || *tool_component_id != *component_id)
                    return std::string("目标面和切割工具必须属于同一个组件。");

                const TopoDS_Shape* tool_shape = ctx.model.geomRegistry().getFace(tool_face_id);
                if (!tool_shape || tool_shape->ShapeType() != TopAbs_FACE)
                    return std::string("所选切割面已失效。");
                splitting_faces.push_back(TopoDS::Face(*tool_shape));
            }
            result = GeometryTopologyEditor::splitFaceByFaces(
                *component->geometry->rootShape, target_face, splitting_faces);
        }

        // 纯 OCC 操作成功并校验后只写回一次，保证一条 undo 记录。
        auto component_operator = ctx.componentOperator(*component_id);
        if (!component_operator)
            return std::string("几何操作失败，详细原因请查看日志。");
        component_operator->replaceGeometryRoot(std::move(result));
        return std::string("分割面成功");
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("SplitFace: {}", detail ? detail : "OpenCASCADE error");
    } catch (const std::exception& error) {
        spdlog::error("SplitFace: {}", error.what());
    }
    return std::string("几何面分割失败，请确认切割工具位于目标面上或与目标面相交；详细原因请查看日志。");
}
}
