#include "ModelData.h"
#include "ModelManager.h"
#include "SplineData.h"
#include <catch2/catch_test_macros.hpp>

#include <memory>

TEST_CASE("ModelData creates single CAD component")
{
    using namespace std;

    auto spline = make_unique<SplineData>();
    spline->rootShape = std::make_unique<TopoDS_Shape>();

    auto model = make_unique<ModelData>(move(spline));

    REQUIRE(model != nullptr);
    REQUIRE(model->components().size() == 1);
    REQUIRE(model->hasSpline());
    REQUIRE(model->type() == ModelData::Type::Spline);

    auto& comps = model->components();
    REQUIRE(comps[0] != nullptr);
    REQUIRE(comps[0]->hasCad());
    REQUIRE(!comps[0]->hasMesh());
}

TEST_CASE("ModelData supports multiple components")
{
    using namespace std;

    auto model = make_unique<ModelData>();
    model->model_name_ = "multi_component_model";

    auto spline1 = make_unique<SplineData>();
    spline1->rootShape = std::make_unique<TopoDS_Shape>();

    auto spline2 = make_unique<SplineData>();
    spline2->rootShape = std::make_unique<TopoDS_Shape>();

    Component* c1 = model->createComponent(-1, "Comp_0");
    c1->cad = move(spline1);

    Component* c2 = model->createComponent(-1, "Comp_1");
    c2->cad = move(spline2);

    REQUIRE(model->components().size() == 2);
    REQUIRE(model->hasSpline());
    REQUIRE(model->type() == ModelData::Type::Spline);

    REQUIRE(model->components()[0] != nullptr);
    REQUIRE(model->components()[1] != nullptr);

    REQUIRE(model->components()[0]->hasCad());
    REQUIRE(model->components()[1]->hasCad());
}

TEST_CASE("ModelManager adds multiple CAD components")
{
    using namespace std;

    ModelManager manager;

    auto model = make_unique<ModelData>();
    model->model_name_ = "component_id_test";

    auto spline1 = make_unique<SplineData>();
    spline1->rootShape = std::make_unique<TopoDS_Shape>();

    auto spline2 = make_unique<SplineData>();
    spline2->rootShape = std::make_unique<TopoDS_Shape>();

    Component* c1 = model->createComponent(-1, "Part_1");
    c1->cad = move(spline1);

    Component* c2 = model->createComponent(-1, "Part_2");
    c2->cad = move(spline2);

    REQUIRE(model->components().size() == 2);

    Index modelId = manager.addModel(move(model));
    REQUIRE(modelId == 0);
    REQUIRE(manager.getModelOperator(modelId));
}