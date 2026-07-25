/**
 * @file TestMeasureHandler.cpp
 * @brief 测量功能处理器的单元测试
 * @author 范成通 email 1941804585@qq.com
 */

#include "MeasureHandler.h"
#include "ArgObject.h"
#include "ComponentData.h"
#include "EventBus.h"
#include "FeatureContext.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "GeometryData.h"
#include "InteractionContext.h"
#include "InteractionState.h"
#include "MakeMeshData.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "Selection.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <TopoDS_Shape.hxx>

#include <catch2/catch_test_macros.hpp>

#include <any>
#include <memory>
#include <optional>
#include <string>

using namespace systems::feature;

namespace {

std::shared_ptr<Selection> makeVertexSelection(const std::vector<Index>& ids)
{
    return std::make_shared<Selection>(Selection { ids, ElementEnum::Vertex, 0 });
}

std::shared_ptr<Selection> makeEdgeSelection(const std::vector<Index>& ids)
{
    return std::make_shared<Selection>(Selection { ids, ElementEnum::Edge, 0 });
}

std::shared_ptr<Selection> makeGeometrySelection(ElementEnum::Type type, const std::vector<Index>& ids)
{
    return std::make_shared<Selection>(Selection { ids, type, 0 });
}

//! @brief 功能测试环境：手工装配 FeatureContext（参数集 + 活动组件 provider），替代原算法 HandlerContext
struct FeatureTestEnv {
    ModelLayer mgr;
    core::EventBus bus;
    MeasureHandler handler;
    FeatureRegistrar registrar;
    std::optional<Index> active_component;
    std::unique_ptr<FeatureParams> params;
    systems::interaction::InteractionState interaction_state_;
    InteractionContext interaction_ctx_;
    std::unique_ptr<FeatureContext> ctx;

    FeatureTestEnv()
        : interaction_ctx_(interaction_state_)
    {
        // 参数声明直接取自被测功能的 setup，避免测试中重复维护一份
        handler.setup(registrar);
        params = std::make_unique<FeatureParams>(registrar.argTypes());
        ctx = std::make_unique<FeatureContext>(FeatureContext {
            mgr,
            bus,
            *params,
            interaction_ctx_,
            []() -> std::optional<Index> { return std::nullopt; },
            [this]() { return active_component; },
            [this](Index component_id) { return mgr.getComponentOperator(component_id); },
        });
    }

    //! @brief 写入功能参数并执行尺寸标注，返回结果字符串（下标 0=测量类型 1=选择对象，见 setup）
    std::string executeMeasure(int measure_type_index, std::shared_ptr<Selection> selection)
    {
        params->setValue(0, core::ArgObject::create<ArgTypeEnum::Combo>(measure_type_index));
        params->setValue(1, core::ArgObject::create<ArgTypeEnum::Selector>(std::move(selection)));
        auto result_any = handler.execute(*ctx);
        if (const std::string* result = std::any_cast<std::string>(&result_any))
            return *result;
        return {};
    }
};

//! @brief 在 10×10×10 的 OCC 立方体几何组件上执行一次测量，返回结果字符串
std::string executeGeometryMeasureOnBox(int measure_type_index, ElementEnum::Type sel_type, int local_id)
{
    BRepPrimAPI_MakeBox box_maker(10.0, 10.0, 10.0);
    box_maker.Build();
    if (!box_maker.IsDone())
        return {};

    auto geometry = std::make_unique<GeometryData>();
    geometry->rootShape = std::make_unique<TopoDS_Shape>(box_maker.Shape());

    auto c = std::make_unique<ComponentData>();
    c->id = -1;
    c->name = "box";
    c->geometry = std::move(geometry);

    ComponentDatas comps;
    comps.push_back(std::move(c));

    FeatureTestEnv env;
    const Index model_id = env.mgr.addModel("box", std::move(comps));
    auto cids = env.mgr.modelById(model_id)->componentIds();
    if (cids.size() != 1)
        return {};

    ComponentData* comp = env.mgr.findComponent(cids[0]);
    if (!comp)
        return {};
    comp->geometry->ensureIndexBuilt(env.mgr.geomRegistry());

    Index gid = -1;
    switch (sel_type) {
    case ElementEnum::GeometryEdge:
        gid = comp->geometry->index.edgeGlobalId(local_id);
        break;
    case ElementEnum::GeometryFace:
        gid = comp->geometry->index.faceGlobalId(local_id);
        break;
    case ElementEnum::GeometrySolid:
        gid = comp->geometry->index.solidGlobalId(local_id);
        break;
    default:
        break;
    }

    auto selection = makeGeometrySelection(sel_type, { gid });
    selection->component_id = cids[0];
    return env.executeMeasure(measure_type_index, selection);
}

} // namespace

TEST_CASE("MeasureHandler setup declares measure parameters and menu")
{
    MeasureHandler handler;
    FeatureRegistrar reg;
    handler.setup(reg);

    REQUIRE(reg.argTypes().size() == 2);
    CHECK(reg.argTypes()[0].type == ArgTypeEnum::Combo);
    CHECK(reg.argTypes()[1].type == ArgTypeEnum::Selector);
    REQUIRE(reg.menuItems().size() == 1);
}

TEST_CASE("MeasureHandler: interactive picks update state annotations and onClear resets")
{
    FeatureTestEnv env;
    env.handler.activate(*env.ctx);
    REQUIRE(env.interaction_state_.on_pick);

    // 两点成线：(0,0,0) → (1,0,0)
    systems::interaction::PickInfo p1;
    p1.valid = true;
    p1.world_pos = { 0.0, 0.0, 0.0 };
    p1.mesh_id = 0;
    systems::interaction::PickInfo p2;
    p2.valid = true;
    p2.world_pos = { 1.0, 0.0, 0.0 };
    p2.mesh_id = 1;

    env.interaction_state_.on_pick(p1);
    CHECK(env.handler.hasPending());
    env.interaction_state_.on_pick(p2);
    CHECK(env.handler.lineCount() == 1);

    // 交互标注写入 InteractionState.annotations（渲染层拉取绘制的契约）
    CHECK(env.interaction_state_.annotations.lines.size() == 1);
    CHECK(env.interaction_state_.annotations.points.size() == 2);
    CHECK(env.interaction_state_.annotations.texts.size() == 1);

    // 面板"清除"：on_clear 清空会话状态与标注
    REQUIRE(env.interaction_state_.on_clear);
    env.interaction_state_.on_clear();
    CHECK(env.handler.lineCount() == 0);
    CHECK(!env.handler.hasPending());
    CHECK(env.interaction_state_.annotations.lines.empty());
    CHECK(env.interaction_state_.annotations.points.empty());
    CHECK(env.interaction_state_.annotations.texts.empty());
}

TEST_CASE("MeasureHandler: distance between two vertices")
{
    auto mesh = std::make_unique<MeshData>(MakeMeshData());
    auto c = std::make_unique<ComponentData>();
    c->id = -1;
    c->name = "Comp";
    c->mesh = std::move(mesh);

    ComponentDatas comps;
    comps.push_back(std::move(c));

    FeatureTestEnv env;
    const Index model_id = env.mgr.addModel("test", std::move(comps));

    auto cids = env.mgr.modelById(model_id)->componentIds();
    REQUIRE(cids.size() == 1);

    ComponentData* comp = env.mgr.findComponent(cids[0]);
    REQUIRE(comp);

    MeshData* mesh_ptr = comp->asMeshData();
    REQUIRE(mesh_ptr);
    const Index base = mesh_ptr->local_to_global_[0];

    auto selection = makeVertexSelection({ base + 0, base + 1 });
    selection->component_id = cids[0];
    const std::string result = env.executeMeasure(0, selection); // 距离
    CHECK(result.find("Distance") != std::string::npos);
    CHECK(result.find("1.000000") != std::string::npos);
}

TEST_CASE("MeasureHandler: length of edges selected as endpoint vertex id pairs")
{
    auto mesh = std::make_unique<MeshData>(MakeMeshData());
    auto c = std::make_unique<ComponentData>();
    c->id = -1;
    c->name = "Comp";
    c->mesh = std::move(mesh);

    ComponentDatas comps;
    comps.push_back(std::move(c));

    FeatureTestEnv env;
    const Index model_id = env.mgr.addModel("test", std::move(comps));

    auto cids = env.mgr.modelById(model_id)->componentIds();
    REQUIRE(cids.size() == 1);

    ComponentData* comp = env.mgr.findComponent(cids[0]);
    REQUIRE(comp);

    MeshData* mesh_ptr = comp->asMeshData();
    REQUIRE(mesh_ptr);
    const Index base = mesh_ptr->local_to_global_[0];

    // 边选择的 ids 是端点顶点 id 对：{12,13} 与 {0,1} 长度均为 1
    auto selection = makeEdgeSelection({ base + 12, base + 13, base + 0, base + 1 });
    selection->component_id = cids[0];
    const std::string result = env.executeMeasure(3, selection); // 长度
    CHECK(result.find("累计长度: 2.000000") != std::string::npos);
}

TEST_CASE("MeasureHandler: angle between two edges selected as endpoint vertex id pairs")
{
    auto mesh = std::make_unique<MeshData>(MakeMeshData());
    auto c = std::make_unique<ComponentData>();
    c->id = -1;
    c->name = "Comp";
    c->mesh = std::move(mesh);

    ComponentDatas comps;
    comps.push_back(std::move(c));

    FeatureTestEnv env;
    const Index model_id = env.mgr.addModel("test", std::move(comps));

    auto cids = env.mgr.modelById(model_id)->componentIds();
    REQUIRE(cids.size() == 1);

    ComponentData* comp = env.mgr.findComponent(cids[0]);
    REQUIRE(comp);

    MeshData* mesh_ptr = comp->asMeshData();
    REQUIRE(mesh_ptr);
    const Index base = mesh_ptr->local_to_global_[0];

    // 边 {0,1} 方向 (1,0,0)，边 {0,3} 方向 (0,1,0)，夹角 90°
    auto selection = makeEdgeSelection({ base + 0, base + 1, base + 0, base + 3 });
    selection->component_id = cids[0];
    const std::string result = env.executeMeasure(1, selection); // 角度
    CHECK(result.find("Angle: 90.000000 deg") != std::string::npos);
}

TEST_CASE("MeasureHandler: geometry edge length on OCC box")
{
    // 10×10×10 立方体任一条几何边长度均为 10
    const std::string result = executeGeometryMeasureOnBox(3, ElementEnum::GeometryEdge, 1);
    CHECK(result.find("累计长度: 10.000000") != std::string::npos);
}

TEST_CASE("MeasureHandler: geometry face area on OCC box")
{
    // 10×10×10 立方体任一个几何面面积均为 100
    const std::string result = executeGeometryMeasureOnBox(4, ElementEnum::GeometryFace, 1);
    CHECK(result.find("总面积: 100.000000") != std::string::npos);
}

TEST_CASE("MeasureHandler: geometry solid volume on OCC box")
{
    // 10×10×10 立方体体积为 1000
    const std::string result = executeGeometryMeasureOnBox(5, ElementEnum::GeometrySolid, 1);
    CHECK(result.find("总体积: 1000.000000") != std::string::npos);
}
