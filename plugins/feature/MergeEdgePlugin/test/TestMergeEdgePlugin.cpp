#include "ComponentData.h"
#include "EventBus.h"
#include "FeatureSystem.h"
#include "GeometryBuilder.h"
#include "GeometryData.h"
#include "MergeEdgeHandler.h"
#include "ModelLayer.h"
#include "ModelOperator.h"
#include "Selection.h"
#include "UndoStack.h"

#include <BRepBuilderAPI_MakeWire.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <NCollection_IndexedMap.hxx>
#include <catch2/catch_test_macros.hpp>

#include <any>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace systems;
using namespace systems::feature;

namespace {
/**
 * @brief 合并边插件测试数据，保存组件与两条待合并边的业务 ID。
 */
struct MergeEdgeFixture {
    Index component_id { -1 };
    std::vector<GeomEdgeId> edge_ids;
};

HandlerMetaData handlerMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "MergeEdge";
    meta_data.display_name = "合并边";
    return meta_data;
}

/**
 * @brief 创建由两条共线边组成的 Wire，并以共享拓扑点连接。
 */
MergeEdgeFixture addMergeableEdges(ModelLayer& model_layer)
{
    const TopoDS_Vertex first = TopoDS::Vertex(GeometryBuilder::makePoint(0.0, 0.0, 0.0));
    const TopoDS_Vertex middle = TopoDS::Vertex(GeometryBuilder::makePoint(10.0, 0.0, 0.0));
    const TopoDS_Vertex last = TopoDS::Vertex(GeometryBuilder::makePoint(20.0, 0.0, 0.0));
    const TopoDS_Edge first_edge = TopoDS::Edge(GeometryBuilder::makeLine(first, middle));
    const TopoDS_Edge second_edge = TopoDS::Edge(GeometryBuilder::makeLine(middle, last));

    BRepBuilderAPI_MakeWire wire_builder;
    wire_builder.Add(first_edge);
    wire_builder.Add(second_edge);
    REQUIRE(wire_builder.IsDone());

    auto geometry = std::make_unique<GeometryData>();
    geometry->setRootShape(wire_builder.Wire());
    auto component = std::make_unique<ComponentData>();
    component->name = "MergeableEdges";
    component->geometry = std::move(geometry);

    const Index model_id = model_layer.addModel("merge_edge_test", {});
    auto model_operator = model_layer.getModelOperator(model_id);
    REQUIRE(model_operator.has_value());
    const Index component_id = model_operator->addGeometryComponent(std::move(component));

    ComponentData* stored = model_layer.findComponent(component_id);
    REQUIRE(stored != nullptr);
    stored->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    const auto& index = stored->geometry->index;
    const int first_local = index.type_maps[index.typeIndex(TopAbs_EDGE)].FindIndex(first_edge);
    const int second_local = index.type_maps[index.typeIndex(TopAbs_EDGE)].FindIndex(second_edge);
    REQUIRE(first_local > 0);
    REQUIRE(second_local > 0);

    return {
        component_id,
        { index.edgeGlobalId(first_local), index.edgeGlobalId(second_local) },
    };
}

std::shared_ptr<Selection> makeEdgeSelection(const MergeEdgeFixture& fixture)
{
    auto selection = std::make_shared<Selection>();
    selection->type = ElementEnum::GeometryEdge;
    selection->component_id = fixture.component_id;
    selection->ids.assign(fixture.edge_ids.begin(), fixture.edge_ids.end());
    return selection;
}

int countEdges(const ComponentData& component)
{
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edges;
    TopExp::MapShapes(*component.geometry->rootShape, TopAbs_EDGE, edges);
    return edges.Extent();
}
}

TEST_CASE("MergeEdge feature merges selected edges and records one undo operation", "[MergeEdgePlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const MergeEdgeFixture fixture = addMergeableEdges(model_layer);
    UndoStack undo_stack(model_layer);
    model_layer.setUndoRecorder(&undo_stack);
    FeatureSystem feature_system(model_layer, bus, &undo_stack);
    undo_stack.clear();
    FeatureSystem::SystemHandlerPtr handler { new MergeEdgeHandler };
    REQUIRE(feature_system.registerHandler(handlerMetaData(), std::move(handler)));
    const auto navigation_entries = feature_system.getNavigationEntries();
    REQUIRE(navigation_entries.size() == 1);
    CHECK(navigation_entries.front().id == "MergeEdge");
    CHECK(navigation_entries.front().title == "合并边");
    CHECK(navigation_entries.front().menu_path == "几何/拓扑");

    const std::any hint = feature_system.invoke("MergeEdge");
    REQUIRE(std::any_cast<std::string>(hint)
        == "请选择两条或多条需要合并的几何边。");
    REQUIRE(feature_system.setParameter("MergeEdge", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(makeEdgeSelection(fixture))));

    const std::string result = std::any_cast<std::string>(feature_system.invoke("MergeEdge"));
    REQUIRE(result.find("成功") != std::string::npos);
    REQUIRE(countEdges(*model_layer.findComponent(fixture.component_id)) == 1);
    REQUIRE(undo_stack.undoLabel() == "合并边");

    undo_stack.undo();
    REQUIRE(countEdges(*model_layer.findComponent(fixture.component_id)) == 2);
    undo_stack.redo();
    REQUIRE(countEdges(*model_layer.findComponent(fixture.component_id)) == 1);
}
