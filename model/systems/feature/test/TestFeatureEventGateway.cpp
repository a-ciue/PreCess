#include "EventBus.h"
#include "FeatureEventGateway.h"
#include "ComponentData.h"
#include "ComponentOperator.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "ModelObserver.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <stdexcept>
#include <string>

using namespace systems::feature;

namespace {
struct CountingObserver : ModelObserver {
    int component_changed_count { 0 };
    Index last_component_changed { -1 };

    void notifyModelChanged(Index) override { }
    void notifyModelAdded(Index) override { }
    void notifyModelRemoved(Index) override { }
    void notifyComponentRemoved(Index) override { }
    void notifyComponentChanged(Index component_id) override
    {
        ++component_changed_count;
        last_component_changed = component_id;
    }
    void notifyModelNameChanged(Index, const std::string&) override { }
    void notifyGeometryLoadFailed(const std::string&) override { }
};

struct TestEvent {
};

//! @brief 构造一个简单三角形面片组件并入池，返回 component_id
Index addTriangleComponent(ModelLayer& mgr)
{
    auto mesh = std::make_unique<MeshData>();
    mesh->init();
    mesh->vertex_positions_ = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
    mesh->face_vertices_ = { 0, 1, 2 };
    mesh->face_vertices_offset_ = { 0, 3 };

    auto c = std::make_unique<ComponentData>();
    c->name = "Comp_0";
    c->mesh = std::move(mesh);
    ComponentDatas comps;
    comps.push_back(std::move(c));

    const Index model_id = mgr.addModel("gateway_test", std::move(comps));
    return mgr.modelById(model_id)->componentIds()[0];
}
}

TEST_CASE("FeatureEventGateway flushes notifications after handler returns", "[FeatureEventGateway]")
{
    CountingObserver obs;
    ModelLayer mgr(&obs);
    core::EventBus bus;
    FeatureEventGateway gateway(bus, mgr);
    const Index component_id = addTriangleComponent(mgr);
    const int count_after_add = obs.component_changed_count;

    // 订阅回调内写模型：写必脏记入待通知集合，回调内不即时通知
    auto sub = gateway.subscribe<TestEvent>([&](const TestEvent&) {
        auto op = mgr.getComponentOperator(component_id);
        REQUIRE(op.has_value());
        op->appendPoint({ 2.0, 0.0, 0.0 });
        REQUIRE(obs.component_changed_count == count_after_add);
    });

    bus.publish(TestEvent {});
    // 回调返回后（操作边界）通知已发
    REQUIRE(obs.component_changed_count == count_after_add + 1);
    REQUIRE(obs.last_component_changed == component_id);
}

TEST_CASE("FeatureEventGateway flushes before rethrowing handler exception", "[FeatureEventGateway]")
{
    CountingObserver obs;
    ModelLayer mgr(&obs);
    core::EventBus bus;
    FeatureEventGateway gateway(bus, mgr);
    const Index component_id = addTriangleComponent(mgr);
    const int count_after_add = obs.component_changed_count;

    // 回调写模型后抛异常：先 flush 再重抛，部分写入的通知不丢
    auto sub = gateway.subscribe<TestEvent>([&](const TestEvent&) {
        auto op = mgr.getComponentOperator(component_id);
        REQUIRE(op.has_value());
        op->appendPoint({ 2.0, 0.0, 0.0 });
        throw std::runtime_error("handler failure");
    });

    REQUIRE_THROWS_AS(bus.publish(TestEvent {}), std::runtime_error);
    REQUIRE(obs.component_changed_count == count_after_add + 1);
    REQUIRE(obs.last_component_changed == component_id);
}

TEST_CASE("FeatureEventGateway flushes nothing when handler does not write", "[FeatureEventGateway]")
{
    CountingObserver obs;
    ModelLayer mgr(&obs);
    core::EventBus bus;
    FeatureEventGateway gateway(bus, mgr);
    addTriangleComponent(mgr);
    const int count_after_add = obs.component_changed_count;

    // 回调不写模型：flush 空转，无通知
    auto sub = gateway.subscribe<TestEvent>([](const TestEvent&) { });
    bus.publish(TestEvent {});
    REQUIRE(obs.component_changed_count == count_after_add);
}

TEST_CASE("flushNotifications is reentrant-safe through observer-event-gateway chain", "[FeatureEventGateway]")
{
    // 复刻生产链路：flush → notifyComponentChanged →（桥接）publish ModelEvent →
    // 网关包装的功能回调 → 重入 flushNotifications。
    // pending 集合若未在通知前清空，将"遍历中重入 flush → 再通知"无限递归（QML 侧 RangeError）。
    struct BridgingObserver : CountingObserver {
        core::EventBus* bus { nullptr };
        void notifyComponentChanged(Index component_id) override
        {
            CountingObserver::notifyComponentChanged(component_id);
            bus->publish(TestEvent {}); //> 模拟 QModelManager 的 ModelEvent 桥接
        }
    };

    BridgingObserver obs;
    ModelLayer mgr(&obs);
    core::EventBus bus;
    obs.bus = &bus;
    FeatureEventGateway gateway(bus, mgr);
    const Index component_id = addTriangleComponent(mgr);
    const int count_after_add = obs.component_changed_count;

    // 网关订阅：回调不写模型，仅经包装逻辑触发重入 flush（集合已空应空转）
    auto sub = gateway.subscribe<TestEvent>([](const TestEvent&) { });

    auto op = mgr.getComponentOperator(component_id);
    REQUIRE(op.has_value());
    op->appendPoint({ 2.0, 0.0, 0.0 });
    mgr.flushNotifications();

    // 恰好通知一次：重入不递归、不重复通知
    REQUIRE(obs.component_changed_count == count_after_add + 1);
    REQUIRE(obs.last_component_changed == component_id);
}
