#include "MakeMeshData.h"
#include "ModelLayer.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("ModelLayer add nullptr")
{
    ModelLayer manager;
    manager.addModel(nullptr);
    manager.addModel(nullptr);
    REQUIRE(!manager.getModelOperator(0));
}

TEST_CASE("ModelLayer add a MeshData")
{
    using namespace std;
    ModelLayer manager;

    auto mesh = make_unique<MeshData>(MakeMeshData());
    auto model = make_unique<ModelData>(move(mesh));

    manager.addModel(move(model));

    SECTION("getModelOperator with invalid id")
    {
        REQUIRE(!manager.getModelOperator(1));
        REQUIRE(!manager.getModelOperator(-1));
    }

    SECTION("getModelOperator with valid id")
    {
        REQUIRE(manager.getModelOperator(0));
    }
}