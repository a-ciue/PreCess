#include "ModelData.h"
#include "ModelLayer.h"
#include "GeometryData.h"

#include <catch2/catch_test_macros.hpp>

#include <BRepPrimAPI_MakeBox.hxx>
#include <TopoDS_Shape.hxx>

TEST_CASE("CAD index build for single component")
{
    using namespace std;

    ModelLayer manager;

    auto spline = make_unique<GeometryData>();
    spline->rootShape = make_unique<TopoDS_Shape>(
        BRepPrimAPI_MakeBox(1.0, 1.0, 1.0).Shape());

    auto model = make_unique<ModelData>(move(spline));

    Index modelId = manager.addModel(move(model));
    REQUIRE(modelId == 0);

    ComponentData* comp = manager.findComponent(0);
    REQUIRE(comp != nullptr);
    REQUIRE(comp->geometry != nullptr);
    REQUIRE(comp->geometry->cad_index.built);

    REQUIRE_FALSE(comp->geometry->cad_index.face_local_to_global.empty());
    REQUIRE_FALSE(comp->geometry->cad_index.edge_local_to_global.empty());
    REQUIRE_FALSE(comp->geometry->cad_index.vertex_local_to_global.empty());
    REQUIRE_FALSE(comp->geometry->cad_index.solid_local_to_global.empty());

    REQUIRE(comp->geometry->cad_index.face_local_to_global.size() > 1);
    REQUIRE(comp->geometry->cad_index.edge_local_to_global.size() > 1);
    REQUIRE(comp->geometry->cad_index.vertex_local_to_global.size() > 1);
    REQUIRE(comp->geometry->cad_index.solid_local_to_global.size() > 1);
}

TEST_CASE("CAD index build for multiple components")
{
    using namespace std;

    ModelLayer manager;

    auto model = make_unique<ModelData>();
    model->model_name_ = "two_boxes";

    auto spline1 = make_unique<GeometryData>();
    spline1->rootShape = make_unique<TopoDS_Shape>(
        BRepPrimAPI_MakeBox(1.0, 1.0, 1.0).Shape());

    auto spline2 = make_unique<GeometryData>();
    spline2->rootShape = make_unique<TopoDS_Shape>(
        BRepPrimAPI_MakeBox(2.0, 1.0, 1.0).Shape());

    ComponentData* c1 = model->createComponent(-1, "Box_1");
    c1->geometry = move(spline1);

    ComponentData* c2 = model->createComponent(-1, "Box_2");
    c2->geometry = move(spline2);

    Index modelId = manager.addModel(move(model));
    REQUIRE(modelId == 0);

    ComponentData* comp0 = manager.findComponent(0);
    ComponentData* comp1 = manager.findComponent(1);

    REQUIRE(comp0 != nullptr);
    REQUIRE(comp1 != nullptr);
    REQUIRE(comp0 != comp1);

    REQUIRE(comp0->geometry != nullptr);
    REQUIRE(comp1->geometry != nullptr);

    REQUIRE(comp0->geometry->cad_index.built);
    REQUIRE(comp1->geometry->cad_index.built);

    REQUIRE_FALSE(comp0->geometry->cad_index.face_local_to_global.empty());
    REQUIRE_FALSE(comp0->geometry->cad_index.edge_local_to_global.empty());
    REQUIRE_FALSE(comp0->geometry->cad_index.vertex_local_to_global.empty());
    REQUIRE_FALSE(comp0->geometry->cad_index.solid_local_to_global.empty());

    REQUIRE_FALSE(comp1->geometry->cad_index.face_local_to_global.empty());
    REQUIRE_FALSE(comp1->geometry->cad_index.edge_local_to_global.empty());
    REQUIRE_FALSE(comp1->geometry->cad_index.vertex_local_to_global.empty());
    REQUIRE_FALSE(comp1->geometry->cad_index.solid_local_to_global.empty());
}