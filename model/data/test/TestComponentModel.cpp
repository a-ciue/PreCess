#include "Component.h"
#include "ModelData.h"
#include "ModelManager.h"
#include "SplineData.h"

#include <TopoDS_Shape.hxx>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <unordered_set>

TEST_CASE("ModelData creates single CAD component (staging)")
{
    using namespace std;

    auto spline = make_unique<SplineData>();
    spline->rootShape = make_unique<TopoDS_Shape>(); // null shape is ok

    auto model = make_unique<ModelData>(move(spline));
    REQUIRE(model != nullptr);

    // 构建期：只看 staging
    auto& comps = model->stagingcomponents();
    REQUIRE(comps.size() == 1);
    REQUIRE(comps[0] != nullptr);
    REQUIRE(comps[0]->hasCad());
    REQUIRE_FALSE(comps[0]->hasMesh());
}

TEST_CASE("ModelData supports multiple staging components")
{
    using namespace std;

    auto model = make_unique<ModelData>();
    model->model_name_ = "multi_component_model";

    auto spline1 = make_unique<SplineData>();
    spline1->rootShape = make_unique<TopoDS_Shape>();

    auto spline2 = make_unique<SplineData>();
    spline2->rootShape = make_unique<TopoDS_Shape>();

    // 直接往 staging 塞 Component（避免依赖 createComponent，如果你还保留 createComponent 也可以用）
    {
        auto c1 = make_unique<Component>();
        c1->id = -1;
        c1->name = "Comp_0";
        c1->cad = move(spline1);
        model->stagingcomponents().push_back(move(c1));
    }
    {
        auto c2 = make_unique<Component>();
        c2->id = -1;
        c2->name = "Comp_1";
        c2->cad = move(spline2);
        model->stagingcomponents().push_back(move(c2));
    }

    REQUIRE(model->stagingcomponents().size() == 2);
    REQUIRE(model->stagingcomponents()[0] != nullptr);
    REQUIRE(model->stagingcomponents()[1] != nullptr);
    REQUIRE(model->stagingcomponents()[0]->hasCad());
    REQUIRE(model->stagingcomponents()[1]->hasCad());
}

TEST_CASE("ModelManager adds multiple CAD components (runtime access by ids)")
{
    using namespace std;

    ModelManager manager;

    auto model = make_unique<ModelData>();
    model->model_name_ = "component_id_test";

    auto spline1 = make_unique<SplineData>();
    spline1->rootShape = make_unique<TopoDS_Shape>();

    auto spline2 = make_unique<SplineData>();
    spline2->rootShape = make_unique<TopoDS_Shape>();

    {
        auto c1 = make_unique<Component>();
        c1->id = -1;
        c1->name = "Part_1";
        c1->cad = move(spline1);
        model->stagingcomponents().push_back(move(c1));
    }
    {
        auto c2 = make_unique<Component>();
        c2->id = -1;
        c2->name = "Part_2";
        c2->cad = move(spline2);
        model->stagingcomponents().push_back(move(c2));
    }

    REQUIRE(model->stagingcomponents().size() == 2);

    Index modelId = manager.addModel(move(model));
    REQUIRE(modelId == 0);

    // 运行期：通过 ids + findComponent 拿回 Component
    auto ids = manager.getComponentIds(modelId);
    REQUIRE(ids.size() == 2);

    unordered_set<Index> uniq(ids.begin(), ids.end());
    REQUIRE(uniq.size() == 2);

    for (Index cid : ids) {
        Component* c = manager.findComponent(cid);
        REQUIRE(c != nullptr);
        REQUIRE(c->hasCad());
    }

    // staging 在运行期应为空（如果你 addModel 里做了 staging.clear）
    // 这里拿不到 model 指针了，不测 staging，只测 manager 的 component 池
}