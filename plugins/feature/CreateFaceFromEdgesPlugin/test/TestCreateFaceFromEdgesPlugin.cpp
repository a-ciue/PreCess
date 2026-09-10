#include "BRepBuilderAPI_MakeEdge.hxx"
#include "BRepBuilderAPI_MakeVertex.hxx"
#include "ComponentData.h"
#include "ComponentOperator.h"
#include "CreateFaceFromEdgesHandler.h"
#include "EventBus.h"
#include "FeatureSystem.h"
#include "GeometryBuilder.h"
#include "GeometryData.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "Selection.h"
#include "TopoDS_Vertex.hxx"
#include "gp_Pnt.hxx"

#include <catch2/catch_test_macros.hpp>

#include <any>

using namespace systems;
using namespace systems::feature;

namespace {
HandlerMetaData handlerMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "CreateFaceFromEdges";
    meta_data.display_name = "选择闭合边创建面";
    return meta_data;
}

/**
 * @brief 构造共享顶点的正方形闭合轮廓（4 条边），返回组件 id
 *
 * 顶点必须拓扑共享（同 TShape），独立构造的边首尾不闭合无法成面。
 */
Index addSquareLoopComponent(ModelLayer& model_layer)
{
    const TopoDS_Vertex v0 = BRepBuilderAPI_MakeVertex(gp_Pnt(0.0, 0.0, 0.0));
    const TopoDS_Vertex v1 = BRepBuilderAPI_MakeVertex(gp_Pnt(10.0, 0.0, 0.0));
    const TopoDS_Vertex v2 = BRepBuilderAPI_MakeVertex(gp_Pnt(10.0, 10.0, 0.0));
    const TopoDS_Vertex v3 = BRepBuilderAPI_MakeVertex(gp_Pnt(0.0, 10.0, 0.0));

    BRepBuilderAPI_MakeEdge b0(v0, v1);
    BRepBuilderAPI_MakeEdge b1(v1, v2);
    BRepBuilderAPI_MakeEdge b2(v2, v3);
    BRepBuilderAPI_MakeEdge b3(v3, v0);

    const Index model_id = model_layer.addModel("temp_Fixture", {});
    auto model_operator = model_layer.getModelOperator(model_id);
    REQUIRE(model_operator.has_value());
    auto geometry = std::make_unique<GeometryData>();
    geometry->setRootShape(b0.Shape());
    auto component = std::make_unique<ComponentData>();
    component->name = "Fixture";
    component->geometry = std::move(geometry);
    const Index component_id = model_operator->addGeometryComponent(std::move(component));
    auto component_operator = model_layer.getComponentOperator(component_id);
    REQUIRE(component_operator.has_value());
    REQUIRE(component_operator->appendGeometryShape(b1.Shape()) >= 0);
    REQUIRE(component_operator->appendGeometryShape(b2.Shape()) >= 0);
    REQUIRE(component_operator->appendGeometryShape(b3.Shape()) >= 0);

    auto* fixture_component = model_layer.findComponent(component_id);
    REQUIRE(fixture_component != nullptr);
    fixture_component->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    // 共享顶点：4 条边、4 个顶点（向量含 0 号保留槽，局部 id 从 1 起）
    REQUIRE(fixture_component->geometry->index.edge_local_to_global.size() == 4 + 1);
    REQUIRE(fixture_component->geometry->index.vertex_local_to_global.size() == 4 + 1);
    return component_id;
}
}

TEST_CASE("CreateFaceFromEdges execute creates face from closed edge loop", "[CreateFaceFromEdgesPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);

    const Index component_id = addSquareLoopComponent(model_layer);
    feature_system.setActiveComponentProvider([component_id]() { return std::optional<Index> { component_id }; });

    FeatureSystem::SystemHandlerPtr handler { new CreateFaceFromEdgesHandler };
    REQUIRE(feature_system.registerHandler(handlerMetaData(), std::move(handler)));

    // 未选择轮廓边时返回界面提示语
    const std::any hint = feature_system.invoke("CreateFaceFromEdges");
    REQUIRE(std::any_cast<std::string>(hint) == "请选择闭合轮廓边。");

    // 选择 4 条闭合轮廓边创建面
    auto* component = model_layer.findComponent(component_id);
    auto selection = std::make_shared<Selection>();
    selection->type = ElementEnum::GeometryEdge;
    const auto& edge_ids = component->geometry->index.edge_local_to_global;
    selection->ids.assign(edge_ids.begin() + 1, edge_ids.end()); // 跳过 0 号保留槽
    selection->component_id = component_id;
    REQUIRE(feature_system.setParameter("CreateFaceFromEdges", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(selection)));

    const Index result_component_id = std::any_cast<Index>(feature_system.invoke("CreateFaceFromEdges"));
    REQUIRE(result_component_id == component_id); // 写回源组件

    // 闭合轮廓成面：面数 0 + 1，边数不变
    component->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    REQUIRE(component->geometry->index.face_local_to_global.size() == 1 + 1);
    REQUIRE(component->geometry->index.edge_local_to_global.size() == 4 + 1);
}
