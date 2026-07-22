#include "ComponentData.h"
#include "ModelLayer.h"
#include "GeometryData.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>

#include <BRepPrimAPI_MakeBox.hxx>
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

    const GeomFaceId comp0FaceId = comp0->geometry->index.faceGlobalId(1);
    const GeomFaceId comp1FaceId = comp1->geometry->index.faceGlobalId(1);
    REQUIRE(manager.findComponentIdByGeometryFaceId(comp0FaceId) == 0);
    REQUIRE(manager.findComponentIdByGeometryFaceId(comp1FaceId) == 1);
}
