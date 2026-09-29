#include "ComponentData.h"
#include "EventBus.h"
#include "FeatureSystem.h"
#include "GeometryBuilder.h"
#include "GeometryData.h"
#include "MergePointsHandler.h"
#include "ModelLayer.h"
#include "ModelOperator.h"
#include "Selection.h"
#include "UndoStack.h"

#include <BRep_Tool.hxx>
#include <BRep_Builder.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopTools_ShapeMapHasher.hxx>
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
 * @brief 点合并插件测试数据，保存组件与两个待合并点的业务 ID。
 */
struct MergePointsFixture {
    Index component_id { -1 };
    std::vector<Index> vertex_ids;
};

HandlerMetaData handlerMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "MergePoints";
    meta_data.display_name = "点合并";
    return meta_data;
}

MergePointsFixture addTwoPoints(ModelLayer& model_layer)
{
    const TopoDS_Vertex first = TopoDS::Vertex(GeometryBuilder::makePoint(0.0, 0.0, 0.0));
    const TopoDS_Vertex second = TopoDS::Vertex(GeometryBuilder::makePoint(0.005, 0.0, 0.0));

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, first);
    builder.Add(compound, second);

    auto geometry = std::make_unique<GeometryData>();
    geometry->setRootShape(compound);
    auto component = std::make_unique<ComponentData>();
    component->name = "TwoPoints";
    component->geometry = std::move(geometry);

    const Index model_id = model_layer.addModel("merge_points_test", {});
    auto model_operator = model_layer.getModelOperator(model_id);
    REQUIRE(model_operator.has_value());
    const Index component_id = model_operator->addGeometryComponent(std::move(component));

    ComponentData* stored = model_layer.findComponent(component_id);
    REQUIRE(stored != nullptr);
    stored->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    const auto& index = stored->geometry->index;
    const int first_local = index.type_maps[index.typeIndex(TopAbs_VERTEX)].FindIndex(first);
    const int second_local = index.type_maps[index.typeIndex(TopAbs_VERTEX)].FindIndex(second);
    REQUIRE(first_local > 0);
    REQUIRE(second_local > 0);
    return {
        component_id,
        { index.vertexGlobalId(first_local), index.vertexGlobalId(second_local) },
    };
}

std::shared_ptr<Selection> makeVertexSelection(
    const MergePointsFixture& fixture,
    Index vertex_id)
{
    auto selection = std::make_shared<Selection>();
    selection->type = ElementEnum::GeometryVertex;
    selection->component_id = fixture.component_id;
    selection->ids = { vertex_id };
    return selection;
}

int countVertices(const ComponentData& component)
{
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> vertices;
    TopExp::MapShapes(*component.geometry->rootShape, TopAbs_VERTEX, vertices);
    return vertices.Extent();
}
}

TEST_CASE("MergePoints feature merges two vertices at midpoint", "[MergePointsPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    UndoStack undo_stack(model_layer);
    model_layer.setUndoRecorder(&undo_stack);
    FeatureSystem feature_system(model_layer, bus, &undo_stack);

    const MergePointsFixture fixture = addTwoPoints(model_layer);
    undo_stack.clear();
    FeatureSystem::SystemHandlerPtr handler { new MergePointsHandler };
    REQUIRE(feature_system.registerHandler(handlerMetaData(), std::move(handler)));

    REQUIRE(feature_system.setParameter("MergePoints", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(
            makeVertexSelection(fixture, fixture.vertex_ids[0]))));
    REQUIRE(feature_system.setParameter("MergePoints", 1,
        core::ArgObject::create<ArgTypeEnum::Selector>(
            makeVertexSelection(fixture, fixture.vertex_ids[1]))));
    REQUIRE(feature_system.setParameter("MergePoints", 2,
        core::ArgObject::create<ArgTypeEnum::Combo>(2)));
    REQUIRE(feature_system.setParameter("MergePoints", 3,
        core::ArgObject::create<ArgTypeEnum::Float>(0.01)));

    const Index result = std::any_cast<Index>(feature_system.invoke("MergePoints"));
    REQUIRE(result == fixture.component_id);
    REQUIRE(countVertices(*model_layer.findComponent(fixture.component_id)) == 1);
    REQUIRE(undo_stack.undoLabel() == "点合并");

    undo_stack.undo();
    REQUIRE(countVertices(*model_layer.findComponent(fixture.component_id)) == 2);
}

TEST_CASE("MergePoints feature rejects points beyond max distance", "[MergePointsPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    UndoStack undo_stack(model_layer);
    model_layer.setUndoRecorder(&undo_stack);
    FeatureSystem feature_system(model_layer, bus, &undo_stack);

    const MergePointsFixture fixture = addTwoPoints(model_layer);
    FeatureSystem::SystemHandlerPtr handler { new MergePointsHandler };
    REQUIRE(feature_system.registerHandler(handlerMetaData(), std::move(handler)));

    REQUIRE(feature_system.setParameter("MergePoints", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(
            makeVertexSelection(fixture, fixture.vertex_ids[0]))));
    REQUIRE(feature_system.setParameter("MergePoints", 1,
        core::ArgObject::create<ArgTypeEnum::Selector>(
            makeVertexSelection(fixture, fixture.vertex_ids[1]))));
    REQUIRE(feature_system.setParameter("MergePoints", 3,
        core::ArgObject::create<ArgTypeEnum::Float>(0.001)));

    const std::any hint = feature_system.invoke("MergePoints");
    REQUIRE(std::any_cast<std::string>(hint) == "两个点的距离超过最大距离。");
    REQUIRE(countVertices(*model_layer.findComponent(fixture.component_id)) == 2);
}
