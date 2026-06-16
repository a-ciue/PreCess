#include "ComponentData.h"
#include "ModelData.h"
#include "ModelLayer.h"
#include "GeometryData.h"

#include <TopoDS_Shape.hxx>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <unordered_set>

TEST_CASE("ModelData creates single Geometry component (staging)")
{
    using namespace std;

    auto geom = make_unique<GeometryData>();
    geom->rootShape = make_unique<TopoDS_Shape>(); // null shape is ok

    auto model = make_unique<ModelData>(move(geom));
    REQUIRE(model != nullptr);

    // 构建期：只看 staging
    auto& comps = model->stagingcomponents();
    REQUIRE(comps.size() == 1);
    REQUIRE(comps[0] != nullptr);
    REQUIRE(comps[0]->hasGeometry());
    REQUIRE_FALSE(comps[0]->hasMesh());
}

TEST_CASE("ModelData supports multiple staging components")
{
    using namespace std;

    auto model = make_unique<ModelData>();
    model->model_name_ = "multi_component_model";

    auto geom1 = make_unique<GeometryData>();
    geom1->rootShape = make_unique<TopoDS_Shape>();

    auto geom2 = make_unique<GeometryData>();
    geom2->rootShape = make_unique<TopoDS_Shape>();

    // 直接往 staging 塞 ComponentData（避免依赖 createComponent，如果你还保留 createComponent 也可以用）
    {
        auto c1 = make_unique<ComponentData>();
        c1->id = -1;
        c1->name = "Comp_0";
        c1->geometry = move(geom1);
        model->stagingcomponents().push_back(move(c1));
    }
    {
        auto c2 = make_unique<ComponentData>();
        c2->id = -1;
        c2->name = "Comp_1";
        c2->geometry = move(geom2);
        model->stagingcomponents().push_back(move(c2));
    }

    REQUIRE(model->stagingcomponents().size() == 2);
    REQUIRE(model->stagingcomponents()[0] != nullptr);
    REQUIRE(model->stagingcomponents()[1] != nullptr);
    REQUIRE(model->stagingcomponents()[0]->hasGeometry());
    REQUIRE(model->stagingcomponents()[1]->hasGeometry());
}

TEST_CASE("ModelLayer adds multiple Geometry components (runtime access by ids)")
{
    using namespace std;

    ModelLayer manager;

    auto model = make_unique<ModelData>();
    model->model_name_ = "component_id_test";

    auto geom1 = make_unique<GeometryData>();
    geom1->rootShape = make_unique<TopoDS_Shape>();

    auto geom2 = make_unique<GeometryData>();
    geom2->rootShape = make_unique<TopoDS_Shape>();

    {
        auto c1 = make_unique<ComponentData>();
        c1->id = -1;
        c1->name = "Part_1";
        c1->geometry = move(geom1);
        model->stagingcomponents().push_back(move(c1));
    }
    {
        auto c2 = make_unique<ComponentData>();
        c2->id = -1;
        c2->name = "Part_2";
        c2->geometry = move(geom2);
        model->stagingcomponents().push_back(move(c2));
    }

    REQUIRE(model->stagingcomponents().size() == 2);

    Index modelId = manager.addModel(move(model));
    REQUIRE(modelId == 0);

    // 运行期：通过 ids + findComponent 拿回 ComponentData
    auto ids = manager.getComponentIds(modelId);
    REQUIRE(ids.size() == 2);

    unordered_set<Index> uniq(ids.begin(), ids.end());
    REQUIRE(uniq.size() == 2);

    for (Index cid : ids) {
        ComponentData* c = manager.findComponent(cid);
        REQUIRE(c != nullptr);
        REQUIRE(c->hasGeometry());
    }

    // staging 在运行期应为空（如果你 addModel 里做了 staging.clear）
    // 这里拿不到 model 指针了，不测 staging，只测 manager 的 component 池
}