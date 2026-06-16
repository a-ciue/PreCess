#include "ModelData.h"
#include "ModelLayer.h"
#include "GeometryData.h"

#include <catch2/catch_test_macros.hpp>

#include <BRepPrimAPI_MakeBox.hxx>
#include <TopoDS_Shape.hxx>

TEST_CASE("Geometry index build for single component")
{
    using namespace std;

    ModelLayer manager;

    auto geom = make_unique<GeometryData>();
    geom->rootShape = make_unique<TopoDS_Shape>(
        BRepPrimAPI_MakeBox(1.0, 1.0, 1.0).Shape());

    auto model = make_unique<ModelData>(move(geom));

    Index modelId = manager.addModel(move(model));
    REQUIRE(modelId == 0);

    ComponentData* comp = manager.findComponent(0);
    REQUIRE(comp != nullptr);
    REQUIRE(comp->geometry != nullptr);
    REQUIRE(comp->geometry->geometry_index.built);

    REQUIRE_FALSE(comp->geometry->geometry_index.face_local_to_global.empty());
    REQUIRE_FALSE(comp->geometry->geometry_index.edge_local_to_global.empty());
    REQUIRE_FALSE(comp->geometry->geometry_index.vertex_local_to_global.empty());
    REQUIRE_FALSE(comp->geometry->geometry_index.solid_local_to_global.empty());

    REQUIRE(comp->geometry->geometry_index.face_local_to_global.size() > 1);
    REQUIRE(comp->geometry->geometry_index.edge_local_to_global.size() > 1);
    REQUIRE(comp->geometry->geometry_index.vertex_local_to_global.size() > 1);
    REQUIRE(comp->geometry->geometry_index.solid_local_to_global.size() > 1);
}

TEST_CASE("Geometry index build for multiple components")
{
    using namespace std;

    ModelLayer manager;

    auto model = make_unique<ModelData>();
    model->model_name_ = "two_boxes";

    auto geom1 = make_unique<GeometryData>();
    geom1->rootShape = make_unique<TopoDS_Shape>(
        BRepPrimAPI_MakeBox(1.0, 1.0, 1.0).Shape());

    auto geom2 = make_unique<GeometryData>();
    geom2->rootShape = make_unique<TopoDS_Shape>(
        BRepPrimAPI_MakeBox(2.0, 1.0, 1.0).Shape());

    ComponentData* c1 = model->createComponent(-1, "Box_1");
    c1->geometry = move(geom1);

    ComponentData* c2 = model->createComponent(-1, "Box_2");
    c2->geometry = move(geom2);

    Index modelId = manager.addModel(move(model));
    REQUIRE(modelId == 0);

    ComponentData* comp0 = manager.findComponent(0);
    ComponentData* comp1 = manager.findComponent(1);

    REQUIRE(comp0 != nullptr);
    REQUIRE(comp1 != nullptr);
    REQUIRE(comp0 != comp1);

    REQUIRE(comp0->geometry != nullptr);
    REQUIRE(comp1->geometry != nullptr);

    REQUIRE(comp0->geometry->geometry_index.built);
    REQUIRE(comp1->geometry->geometry_index.built);

    REQUIRE_FALSE(comp0->geometry->geometry_index.face_local_to_global.empty());
    REQUIRE_FALSE(comp0->geometry->geometry_index.edge_local_to_global.empty());
    REQUIRE_FALSE(comp0->geometry->geometry_index.vertex_local_to_global.empty());
    REQUIRE_FALSE(comp0->geometry->geometry_index.solid_local_to_global.empty());

    REQUIRE_FALSE(comp1->geometry->geometry_index.face_local_to_global.empty());
    REQUIRE_FALSE(comp1->geometry->geometry_index.edge_local_to_global.empty());
    REQUIRE_FALSE(comp1->geometry->geometry_index.vertex_local_to_global.empty());
    REQUIRE_FALSE(comp1->geometry->geometry_index.solid_local_to_global.empty());
}