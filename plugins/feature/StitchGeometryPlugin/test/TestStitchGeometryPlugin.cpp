#include "ComponentData.h"
#include "EventBus.h"
#include "FeatureSystem.h"
#include "GeometryBuilder.h"
#include "GeometryData.h"
#include "ModelLayer.h"
#include "ModelOperator.h"
#include "Selection.h"
#include "StitchGeometryHandler.h"
#include "UndoStack.h"

#include <BRep_Tool.hxx>
#include <BRep_Builder.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <NCollection_IndexedMap.hxx>
#include <NCollection_List.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <gp_Pnt.hxx>
#include <catch2/catch_test_macros.hpp>

#include <any>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace systems;
using namespace systems::feature;

namespace {
/**
 * @brief 缝合插件测试数据，保存组件以及两组待缝合拓扑的业务 ID。
 */
struct StitchFixture {
    Index component_id { -1 };
    ElementEnum::Type type { ElementEnum::None };
    std::vector<Index> first_ids;
    std::vector<Index> second_ids;
};

HandlerMetaData handlerMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "StitchGeometry";
    meta_data.display_name = "几何缝合";
    return meta_data;
}

std::shared_ptr<Selection> makeSelection(
    const StitchFixture& fixture,
    const std::vector<Index>& ids)
{
    auto selection = std::make_shared<Selection>();
    selection->type = fixture.type;
    selection->component_id = fixture.component_id;
    selection->ids = ids;
    return selection;
}

TopoDS_Edge findEdgeOnX(const TopoDS_Face& face, double x)
{
    constexpr double tolerance = 1.0e-7;
    for (TopExp_Explorer edge_exp(face, TopAbs_EDGE); edge_exp.More(); edge_exp.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(edge_exp.Current());
        TopoDS_Vertex start;
        TopoDS_Vertex end;
        TopExp::Vertices(edge, start, end, true);
        if (!start.IsNull() && !end.IsNull()
            && std::abs(BRep_Tool::Pnt(start).X() - x) <= tolerance
            && std::abs(BRep_Tool::Pnt(end).X() - x) <= tolerance)
            return edge;
    }
    return {};
}

Index addGeometryComponent(
    ModelLayer& model_layer,
    const std::string& name,
    TopoDS_Shape root)
{
    auto geometry = std::make_unique<GeometryData>();
    geometry->setRootShape(std::move(root));
    auto component = std::make_unique<ComponentData>();
    component->name = name;
    component->geometry = std::move(geometry);

    const Index model_id = model_layer.addModel(name, {});
    auto model_operator = model_layer.getModelOperator(model_id);
    REQUIRE(model_operator.has_value());
    return model_operator->addGeometryComponent(std::move(component));
}

StitchFixture addBoundaryGap(ModelLayer& model_layer)
{
    const TopoDS_Face left = TopoDS::Face(GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 10.0, CoordinatePlane::XY));
    const TopoDS_Face right = TopoDS::Face(GeometryBuilder::makeRectangleFace(
        10.005, 0.0, 0.0, 10.0, 10.0, CoordinatePlane::XY));
    const TopoDS_Edge left_boundary = findEdgeOnX(left, 10.0);
    const TopoDS_Edge right_boundary = findEdgeOnX(right, 10.005);
    REQUIRE_FALSE(left_boundary.IsNull());
    REQUIRE_FALSE(right_boundary.IsNull());

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, left);
    builder.Add(compound, right);
    const Index component_id = addGeometryComponent(model_layer, "StitchEdges", compound);

    ComponentData* stored = model_layer.findComponent(component_id);
    REQUIRE(stored != nullptr);
    stored->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    const auto& index = stored->geometry->index;
    const int left_local = index.type_maps[index.typeIndex(TopAbs_EDGE)].FindIndex(left_boundary);
    const int right_local = index.type_maps[index.typeIndex(TopAbs_EDGE)].FindIndex(right_boundary);
    REQUIRE(left_local > 0);
    REQUIRE(right_local > 0);
    return { component_id, ElementEnum::GeometryEdge,
        { index.edgeGlobalId(left_local) }, { index.edgeGlobalId(right_local) } };
}

StitchFixture addNearbyVertices(ModelLayer& model_layer)
{
    const TopoDS_Vertex first = TopoDS::Vertex(GeometryBuilder::makePoint(0.0, 0.0, 0.0));
    const TopoDS_Vertex second = TopoDS::Vertex(GeometryBuilder::makePoint(0.005, 0.0, 0.0));
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, first);
    builder.Add(compound, second);
    const Index component_id = addGeometryComponent(model_layer, "StitchVertices", compound);

    ComponentData* stored = model_layer.findComponent(component_id);
    REQUIRE(stored != nullptr);
    stored->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    const auto& index = stored->geometry->index;
    const int first_local = index.type_maps[index.typeIndex(TopAbs_VERTEX)].FindIndex(first);
    const int second_local = index.type_maps[index.typeIndex(TopAbs_VERTEX)].FindIndex(second);
    REQUIRE(first_local > 0);
    REQUIRE(second_local > 0);
    return { component_id, ElementEnum::GeometryVertex,
        { index.vertexGlobalId(first_local) }, { index.vertexGlobalId(second_local) } };
}

int countSharedEdges(const ComponentData& component)
{
    NCollection_IndexedDataMap<TopoDS_Shape,
        NCollection_List<TopoDS_Shape>, TopTools_ShapeMapHasher>
        edge_faces;
    TopExp::MapShapesAndUniqueAncestors(
        *component.geometry->rootShape, TopAbs_EDGE, TopAbs_FACE, edge_faces);
    int count = 0;
    for (int index = 1; index <= edge_faces.Extent(); ++index) {
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> faces;
        for (const TopoDS_Shape& face : edge_faces.FindFromIndex(index))
            faces.Add(face);
        if (faces.Extent() == 2)
            ++count;
    }
    return count;
}

int countVertices(const ComponentData& component)
{
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> vertices;
    TopExp::MapShapes(*component.geometry->rootShape, TopAbs_VERTEX, vertices);
    return vertices.Extent();
}
}

TEST_CASE("StitchGeometry feature stitches selected boundary edges and supports undo", "[StitchGeometryPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    UndoStack undo_stack(model_layer);
    model_layer.setUndoRecorder(&undo_stack);
    FeatureSystem feature_system(model_layer, bus, &undo_stack);

    const StitchFixture fixture = addBoundaryGap(model_layer);
    undo_stack.clear();
    FeatureSystem::SystemHandlerPtr handler { new StitchGeometryHandler };
    REQUIRE(feature_system.registerHandler(handlerMetaData(), std::move(handler)));
    REQUIRE(std::any_cast<std::string>(feature_system.invoke("StitchGeometry"))
        == "请选择第一个点，或第一组连续自由边。");
    REQUIRE(feature_system.setParameter("StitchGeometry", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(
            makeSelection(fixture, fixture.first_ids))));
    REQUIRE(feature_system.setParameter("StitchGeometry", 1,
        core::ArgObject::create<ArgTypeEnum::Selector>(
            makeSelection(fixture, fixture.second_ids))));
    REQUIRE(feature_system.setParameter("StitchGeometry", 2,
        core::ArgObject::create<ArgTypeEnum::Float>(0.01)));

    const Index result = std::any_cast<Index>(feature_system.invoke("StitchGeometry"));
    REQUIRE(result == fixture.component_id);
    REQUIRE(countSharedEdges(*model_layer.findComponent(fixture.component_id)) == 1);
    REQUIRE(undo_stack.undoLabel() == "几何缝合");

    undo_stack.undo();
    REQUIRE(countSharedEdges(*model_layer.findComponent(fixture.component_id)) == 0);
    undo_stack.redo();
    REQUIRE(countSharedEdges(*model_layer.findComponent(fixture.component_id)) == 1);
}

TEST_CASE("StitchGeometry feature merges two selected vertices at midpoint", "[StitchGeometryPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    UndoStack undo_stack(model_layer);
    model_layer.setUndoRecorder(&undo_stack);
    FeatureSystem feature_system(model_layer, bus, &undo_stack);

    const StitchFixture fixture = addNearbyVertices(model_layer);
    undo_stack.clear();
    FeatureSystem::SystemHandlerPtr handler { new StitchGeometryHandler };
    REQUIRE(feature_system.registerHandler(handlerMetaData(), std::move(handler)));
    REQUIRE(feature_system.setParameter("StitchGeometry", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(
            makeSelection(fixture, fixture.first_ids))));
    REQUIRE(feature_system.setParameter("StitchGeometry", 1,
        core::ArgObject::create<ArgTypeEnum::Selector>(
            makeSelection(fixture, fixture.second_ids))));
    REQUIRE(feature_system.setParameter("StitchGeometry", 2,
        core::ArgObject::create<ArgTypeEnum::Float>(0.02)));
    REQUIRE(feature_system.setParameter("StitchGeometry", 3,
        core::ArgObject::create<ArgTypeEnum::Combo>(2)));

    REQUIRE(std::any_cast<Index>(feature_system.invoke("StitchGeometry"))
        == fixture.component_id);
    REQUIRE(model_layer.geometryCleanupTolerance() == 0.02);
    const ComponentData* component = model_layer.findComponent(fixture.component_id);
    REQUIRE(countVertices(*component) == 1);
    TopExp_Explorer vertex_exp(*component->geometry->rootShape, TopAbs_VERTEX);
    REQUIRE(vertex_exp.More());
    REQUIRE(BRep_Tool::Pnt(TopoDS::Vertex(vertex_exp.Current()))
                .Distance(gp_Pnt(0.0025, 0.0, 0.0))
        < 1.0e-7);
}
