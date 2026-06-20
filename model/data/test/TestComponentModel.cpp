#include "ComponentData.h"
#include "ModelLayer.h"
#include "GeometryData.h"

#include <TopoDS_Shape.hxx>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <unordered_set>

TEST_CASE("ModelData creates single Geometry component")
{
    using namespace std;

    auto geom = make_unique<GeometryData>();
    geom->rootShape = make_unique<TopoDS_Shape>(); // null shape is ok

    ComponentDatas comps;
    auto c = make_unique<ComponentData>();
    c->id = -1;
    c->name = "Comp_0";
    c->geometry = move(geom);
    comps.push_back(move(c));

    REQUIRE(comps.size() == 1);
    REQUIRE(comps[0] != nullptr);
    REQUIRE(comps[0]->hasGeometry());
    REQUIRE_FALSE(comps[0]->hasMesh());
}

TEST_CASE("ModelData supports multiple components")
{
    using namespace std;

    auto geom1 = make_unique<GeometryData>();
    geom1->rootShape = make_unique<TopoDS_Shape>();

    auto geom2 = make_unique<GeometryData>();
    geom2->rootShape = make_unique<TopoDS_Shape>();

    ComponentDatas comps;
    {
        auto c1 = make_unique<ComponentData>();
        c1->id = -1;
        c1->name = "Comp_0";
        c1->geometry = move(geom1);
        comps.push_back(move(c1));
    }
    {
        auto c2 = make_unique<ComponentData>();
        c2->id = -1;
        c2->name = "Comp_1";
        c2->geometry = move(geom2);
        comps.push_back(move(c2));
    }

    REQUIRE(comps.size() == 2);
    REQUIRE(comps[0] != nullptr);
    REQUIRE(comps[1] != nullptr);
    REQUIRE(comps[0]->hasGeometry());
    REQUIRE(comps[1]->hasGeometry());
}

TEST_CASE("ModelLayer adds multiple Geometry components (runtime access by ids)")
{
    using namespace std;

    ModelLayer manager;

    auto geom1 = make_unique<GeometryData>();
    geom1->rootShape = make_unique<TopoDS_Shape>();

    auto geom2 = make_unique<GeometryData>();
    geom2->rootShape = make_unique<TopoDS_Shape>();

    ComponentDatas comps;
    {
        auto c1 = make_unique<ComponentData>();
        c1->id = -1;
        c1->name = "Part_1";
        c1->geometry = move(geom1);
        comps.push_back(move(c1));
    }
    {
        auto c2 = make_unique<ComponentData>();
        c2->id = -1;
        c2->name = "Part_2";
        c2->geometry = move(geom2);
        comps.push_back(move(c2));
    }

    REQUIRE(comps.size() == 2);

    Index modelId = manager.addModel("component_id_test", move(comps));
    REQUIRE(modelId == 0);

    auto ids = manager.modelById(modelId)->componentIds();
    REQUIRE(ids.size() == 2);

    unordered_set<Index> uniq(ids.begin(), ids.end());
    REQUIRE(uniq.size() == 2);

    for (Index cid : ids) {
        ComponentData* c = manager.findComponent(cid);
        REQUIRE(c != nullptr);
        REQUIRE(c->hasGeometry());
    }
}
