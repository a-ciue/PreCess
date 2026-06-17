#include "ComponentData.h"
#include "MakeMeshData.h"
#include "ModelLayer.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("ModelLayer add with empty components")
{
    ModelLayer manager;
    Index id = manager.addModel("empty_model", ComponentDatas{});
    REQUIRE(manager.getModelOperator(id));
    REQUIRE_FALSE(manager.getModelOperator(-1));
    REQUIRE(manager.modelById(id)->componentIds().empty());
}

TEST_CASE("ModelLayer add a MeshData component")
{
    using namespace std;
    ModelLayer manager;

    auto mesh = make_unique<MeshData>(MakeMeshData());
    auto c = make_unique<ComponentData>();
    c->id = -1;
    c->name = "Comp_0";
    c->mesh = move(mesh);
    ComponentDatas comps;
    comps.push_back(move(c));

    Index id = manager.addModel("test_model", move(comps));

    SECTION("getModelOperator with invalid id")
    {
        REQUIRE(!manager.getModelOperator(-1));
    }

    SECTION("getModelOperator with valid id")
    {
        REQUIRE(manager.getModelOperator(id));
    }
}
