/**
 * @file TestDimensionHandler.cpp
 * @brief 尺寸标注处理器的单元测试
 * @author 范成通 email 1941804585@qq.com
 */

#include "DimensionHandler.h"
#include "ArgObject.h"
#include "ComponentData.h"
#include "EventBus.h"
#include "FeatureContext.h"
#include "FeatureEventGateway.h"
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
    FeatureEventGateway gateway { bus, mgr }; //> 事件网关（声明顺序须在 mgr/bus 之后）
    DimensionHandler handler;
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
            gateway,
            *params,
            interaction_ctx_,
            []() -> std::optional<Index> { return std::nullopt; },
            [this]() { return active_component; },
            [this](Index component_id) { return mgr.getComponentOperator(component_id); },
        });
    }

    //! @brief 写入功能参数并执行尺寸标注，返回结果字符串（下标 0=测量类型 1=选择对象，见 setup）
    std::string executeDimension(int measure_type_index, std::shared_ptr<Selection> selection)
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
    return env.executeDimension(measure_type_index, selection);
}

} // namespace

TEST_CASE("DimensionHandler setup declares measure parameters and menu")
{
    DimensionHandler handler;
    FeatureRegistrar reg;
    handler.setup(reg);

    REQUIRE(reg.argTypes().size() == 2);
    CHECK(reg.argTypes()[0].type == ArgTypeEnum::Combo);
    CHECK(reg.argTypes()[1].type == ArgTypeEnum::Selector);
    REQUIRE(reg.menuItems().size() == 1);
}

TEST_CASE("DimensionHandler: distance between two vertices")
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

    // 选择集顶点 id 为全局点 id，取组件入池时分配的真实 gid
    auto selection = makeVertexSelection({ comp->point_global_ids_[0], comp->point_global_ids_[1] });
    selection->component_id = cids[0];
    const std::string result = env.executeDimension(0, selection); // 距离
    CHECK(result.find("Distance") != std::string::npos);
    CHECK(result.find("1.000000") != std::string::npos);
}

TEST_CASE("DimensionHandler: length of edges selected as endpoint vertex id pairs")
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

    // 边选择的 ids 是端点顶点 id 对（全局点 id）：{12,13} 与 {0,1} 长度均为 1
    auto selection = makeEdgeSelection({ comp->point_global_ids_[12], comp->point_global_ids_[13],
        comp->point_global_ids_[0], comp->point_global_ids_[1] });
    selection->component_id = cids[0];
    const std::string result = env.executeDimension(3, selection); // 长度
    CHECK(result.find("累计长度: 2.000000") != std::string::npos);
}

TEST_CASE("DimensionHandler: angle between two edges selected as endpoint vertex id pairs")
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

    // 边 {0,1} 方向 (1,0,0)，边 {0,3} 方向 (0,1,0)，夹角 90°（id 为全局点 id）
    auto selection = makeEdgeSelection({ comp->point_global_ids_[0], comp->point_global_ids_[1],
        comp->point_global_ids_[0], comp->point_global_ids_[3] });
    selection->component_id = cids[0];
    const std::string result = env.executeDimension(1, selection); // 角度
    CHECK(result.find("Angle: 90.000000 deg") != std::string::npos);
}

TEST_CASE("DimensionHandler: geometry edge length on OCC box")
{
    // 10×10×10 立方体任一条几何边长度均为 10
    const std::string result = executeGeometryMeasureOnBox(3, ElementEnum::GeometryEdge, 1);
    CHECK(result.find("累计长度: 10.000000") != std::string::npos);
}

TEST_CASE("DimensionHandler: geometry face area on OCC box")
{
    // 10×10×10 立方体任一个几何面面积均为 100
    const std::string result = executeGeometryMeasureOnBox(4, ElementEnum::GeometryFace, 1);
    CHECK(result.find("总面积: 100.000000") != std::string::npos);
}

TEST_CASE("DimensionHandler: geometry solid volume on OCC box")
{
    // 10×10×10 立方体体积为 1000
    const std::string result = executeGeometryMeasureOnBox(5, ElementEnum::GeometrySolid, 1);
    CHECK(result.find("总体积: 1000.000000") != std::string::npos);
}
