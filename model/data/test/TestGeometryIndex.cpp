#include "ComponentData.h"
#include "ModelLayer.h"
#include "GeometryData.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <gp_Pnt.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopoDS_Shape.hxx>

TEST_CASE("Geometry index build for single component")
{
    using namespace std;

    ModelLayer manager;

    auto geom = make_unique<GeometryData>();
    geom->rootShape = make_unique<TopoDS_Shape>(
        BRepPrimAPI_MakeBox(1.0, 1.0, 1.0).Shape());

    ComponentDatas comps;
    auto c = make_unique<ComponentData>();
    c->id = -1;
    c->name = "Comp_0";
    c->geometry = move(geom);
    comps.push_back(move(c));

    Index modelId = manager.addModel("geom_index_test", move(comps));
    REQUIRE(modelId == 0);

    ComponentData* comp = manager.findComponent(0);
    REQUIRE(comp != nullptr);
    REQUIRE(comp->geometry != nullptr);
    REQUIRE(comp->geometry->index.built);

    REQUIRE_FALSE(comp->geometry->index.face_local_to_global.empty());
    REQUIRE_FALSE(comp->geometry->index.edge_local_to_global.empty());
    REQUIRE_FALSE(comp->geometry->index.vertex_local_to_global.empty());
    REQUIRE_FALSE(comp->geometry->index.solid_local_to_global.empty());

    REQUIRE(comp->geometry->index.face_local_to_global.size() > 1);
    REQUIRE(comp->geometry->index.edge_local_to_global.size() > 1);
    REQUIRE(comp->geometry->index.vertex_local_to_global.size() > 1);
    REQUIRE(comp->geometry->index.solid_local_to_global.size() > 1);
}

TEST_CASE("Geometry index build for multiple components")
{
    using namespace std;

    ModelLayer manager;

    auto geom1 = make_unique<GeometryData>();
    geom1->rootShape = make_unique<TopoDS_Shape>(
        BRepPrimAPI_MakeBox(1.0, 1.0, 1.0).Shape());

    auto geom2 = make_unique<GeometryData>();
    geom2->rootShape = make_unique<TopoDS_Shape>(
        BRepPrimAPI_MakeBox(2.0, 1.0, 1.0).Shape());

    ComponentDatas comps;
    {
        auto c1 = make_unique<ComponentData>();
        c1->id = -1;
        c1->name = "Box_1";
        c1->geometry = move(geom1);
        comps.push_back(move(c1));
    }
    {
        auto c2 = make_unique<ComponentData>();
        c2->id = -1;
        c2->name = "Box_2";
        c2->geometry = move(geom2);
        comps.push_back(move(c2));
    }

    Index modelId = manager.addModel("two_boxes", move(comps));
    REQUIRE(modelId == 0);

    ComponentData* comp0 = manager.findComponent(0);
    ComponentData* comp1 = manager.findComponent(1);

    REQUIRE(comp0 != nullptr);
    REQUIRE(comp1 != nullptr);
    REQUIRE(comp0 != comp1);

    REQUIRE(comp0->geometry != nullptr);
    REQUIRE(comp1->geometry != nullptr);

    REQUIRE(comp0->geometry->index.built);
    REQUIRE(comp1->geometry->index.built);

    REQUIRE_FALSE(comp0->geometry->index.face_local_to_global.empty());
    REQUIRE_FALSE(comp0->geometry->index.edge_local_to_global.empty());
    REQUIRE_FALSE(comp0->geometry->index.vertex_local_to_global.empty());
    REQUIRE_FALSE(comp0->geometry->index.solid_local_to_global.empty());

    REQUIRE_FALSE(comp1->geometry->index.face_local_to_global.empty());
    REQUIRE_FALSE(comp1->geometry->index.edge_local_to_global.empty());
    REQUIRE_FALSE(comp1->geometry->index.vertex_local_to_global.empty());
    REQUIRE_FALSE(comp1->geometry->index.solid_local_to_global.empty());
}

TEST_CASE("Add geometry component to an existing model")
{
    using namespace std;

    ModelLayer manager;
    const Index model_id = manager.addModel("geometry", ComponentDatas {});

    auto geometry = make_unique<GeometryData>();
    geometry->rootShape = make_unique<TopoDS_Shape>(
        BRepPrimAPI_MakeBox(1.0, 2.0, 3.0).Shape());

    auto component = make_unique<ComponentData>();
    component->name = "Box_1";
    component->geometry = move(geometry);

    const Index component_id = manager.addGeometryComponent(model_id, move(component));
    ComponentData* inserted = manager.findComponent(component_id);

    REQUIRE(inserted != nullptr);
    REQUIRE(inserted->geometry != nullptr);
    REQUIRE(inserted->geometry->index.built);
    REQUIRE(manager.modelById(model_id)->componentIds() == vector<Index> { component_id });
}

TEST_CASE("Append geometry shapes to an existing component")
{
    using namespace std;

    ModelLayer manager;
    auto geometry = make_unique<GeometryData>();
    geometry->rootShape = make_unique<TopoDS_Shape>(
        BRepPrimAPI_MakeBox(1.0, 1.0, 1.0).Shape());

    auto component = make_unique<ComponentData>();
    component->name = "Geometry_1";
    component->geometry = move(geometry);

    ComponentDatas components;
    components.push_back(move(component));
    const Index model_id = manager.addModel("geometry", move(components));
    const Index component_id = manager.modelById(model_id)->componentIds().front();

    manager.appendGeometryShape(
        component_id, BRepBuilderAPI_MakeVertex(gp_Pnt(2.0, 2.0, 2.0)).Shape());
    manager.appendGeometryShape(
        component_id, BRepBuilderAPI_MakeVertex(gp_Pnt(3.0, 3.0, 3.0)).Shape());

    ComponentData* updated = manager.findComponent(component_id);
    REQUIRE(updated != nullptr);
    REQUIRE(updated->geometry->rootShape->ShapeType() == TopAbs_COMPOUND);
    REQUIRE(updated->geometry->index.built);
    REQUIRE(updated->geometry->index.type_maps[
        GeometrySubshapeIndex::typeIndex(TopAbs_VERTEX)].Extent() == 10);

    // Box 与两个 Point 应作为根 Compound 的三个直接子形状。
    int direct_child_count = 0;
    for (TopoDS_Iterator it(*updated->geometry->rootShape); it.More(); it.Next())
        ++direct_child_count;
    REQUIRE(direct_child_count == 3);
    REQUIRE(manager.modelById(model_id)->componentIds() == vector<Index> { component_id });
}

TEST_CASE("Initialize and append geometry in a mesh-only component")
{
    using namespace std;

    ModelLayer manager;
    auto component = make_unique<ComponentData>();
    component->name = "Mesh_1";
    component->mesh = make_unique<MeshData>();

    ComponentDatas components;
    components.push_back(move(component));
    const Index model_id = manager.addModel("mesh", move(components));
    const Index component_id = manager.modelById(model_id)->componentIds().front();

    manager.appendGeometryShape(
        component_id, BRepBuilderAPI_MakeVertex(gp_Pnt(1.0, 2.0, 3.0)).Shape());
    manager.appendGeometryShape(
        component_id, BRepBuilderAPI_MakeVertex(gp_Pnt(4.0, 5.0, 6.0)).Shape());

    ComponentData* updated = manager.findComponent(component_id);
    REQUIRE(updated != nullptr);
    REQUIRE(updated->mesh != nullptr);
    REQUIRE(updated->geometry != nullptr);
    REQUIRE(updated->geometry->rootShape != nullptr);
    REQUIRE(updated->geometry->rootShape->ShapeType() == TopAbs_COMPOUND);
    REQUIRE(updated->geometry->index.built);
    REQUIRE(updated->geometry->index.type_maps[
        GeometrySubshapeIndex::typeIndex(TopAbs_VERTEX)].Extent() == 2);
}
