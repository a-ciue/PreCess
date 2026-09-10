#include "CreateFaceFromEdgesHandler.h"
#include "ComponentData.h"
#include "GeometryData.h"
#include "FeatureContext.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "GeometryBuilder.h"

#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <algorithm>
#include <spdlog/spdlog.h>
#include <vector>

namespace systems::feature {
void CreateFaceFromEdgesHandler::setup(FeatureRegistrar& reg, FeatureContext& /*ctx*/)
{
    // 参数声明与原 GeometryOperationActions.qml 中 createFaceFromEdgesInfo 保持一致；
    // 菜单与图标复用原"几何"页"闭合边成面"按钮的声明
    reg.addParameter({ ArgTypeEnum::Selector, "轮廓边", "", "请选择一条或多条闭合轮廓边" });
    reg.addMenuItem({ "几何", "选择闭合边创建面", "qrc:/images/toolbar/Geometry/close_edges_to_form_surface.svg" });
}

std::any CreateFaceFromEdgesHandler::execute(FeatureContext& ctx)
{
    // 依赖已有拓扑的操作写回其来源 Component（对象树选中态提示）
    const auto component_id = ctx.activeComponent ? ctx.activeComponent() : std::nullopt;
    if (!component_id)
        return std::string("请先选择目标 Component。");

    const auto* selection_param = ctx.params.value(0).get<ArgTypeEnum::Selector>();
    if (!selection_param || !*selection_param)
        return std::string("请选择闭合轮廓边。");
    const std::shared_ptr<Selection>& selected = *selection_param;
    if (selected->type != ElementEnum::GeometryEdge || selected->ids.empty())
        return std::string("请选择闭合轮廓边。");

    try {
        ComponentData* component = ctx.model.findComponent(*component_id);
        if (!component || !component->geometry || !component->geometry->rootShape)
            return std::string("目标组件没有几何。");
        component->geometry->ensureIndexBuilt(ctx.model.geomRegistry());

        const auto& component_edge_ids = component->geometry->index.edge_local_to_global;
        std::vector<TopoDS_Edge> edges;
        edges.reserve(selected->ids.size());
        for (Index id : selected->ids) {
            if (std::find(component_edge_ids.begin(), component_edge_ids.end(), id)
                == component_edge_ids.end())
                return std::string("所选轮廓边必须属于目标组件。");

            const TopoDS_Shape* edge_shape = ctx.model.geomRegistry().getEdge(id);
            if (!edge_shape || edge_shape->ShapeType() != TopAbs_EDGE)
                return std::string("所选轮廓边已失效。");
            // 追加 Face 会重建索引，因此先复制轻量 TopoDS_Edge 句柄。
            edges.push_back(TopoDS::Edge(*edge_shape));
        }

        TopoDS_Shape face = GeometryBuilder::makeFaceFromEdges(edges);
        auto component_operator = ctx.model.getComponentOperator(*component_id);
        if (!component_operator)
            return std::string("几何操作失败，详细原因请查看日志。");
        return component_operator->appendGeometryShape(std::move(face));
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("CreateFaceFromEdges: {}", detail ? detail : "OpenCASCADE error");
    } catch (const std::exception& error) {
        spdlog::error("CreateFaceFromEdges: {}", error.what());
    }
    return std::string("几何操作失败，详细原因请查看日志。");
}
}
