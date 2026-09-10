/**
 * @file TestSession.cpp
 * @brief Session 组合根测试（子系统装配、观察者转发与 ModelEvent 桥接、有序拆解）
 */
#include "Session.h"

#include "ComponentData.h"
#include "EventBus.h"
#include "FeatureEvents.h"
#include "GeometryBuilder.h"
#include "GeometryData.h"
#include "ModelObserver.h"
#include "ModelOperator.h"
#include "UndoStack.h"

#include <catch2/catch_test_macros.hpp>

using namespace session;

namespace {
// 记录各类模型层通知次数的观察者桩
class RecordingObserver : public ModelObserver {
public:
    int model_added { 0 };
    int model_removed { 0 };
    int model_changed { 0 };
    int model_name_changed { 0 };
    int component_changed { 0 };
    int component_removed { 0 };
    int geometry_load_failed { 0 };

    void notifyModelAdded(Index) override { ++model_added; }
    void notifyModelRemoved(Index) override { ++model_removed; }
    void notifyModelChanged(Index) override { ++model_changed; }
    void notifyModelNameChanged(Index, const std::string&) override { ++model_name_changed; }
    void notifyComponentChanged(Index) override { ++component_changed; }
    void notifyComponentRemoved(Index) override { ++component_removed; }
    void notifyGeometryLoadFailed(const std::string&) override { ++geometry_load_failed; }
};

// 统计 ModelEvent 各类事件次数
struct EventCounter {
    int added { 0 };
    int removed { 0 };
    int component_removed { 0 };
    core::EventBus::Subscription sub_; //> 订阅句柄，析构自动退订

    void subscribe(core::EventBus& bus)
    {
        sub_ = bus.subscribe<systems::feature::ModelEvent>([this](const systems::feature::ModelEvent& e) {
            using Kind = systems::feature::ModelEvent::Kind;
            if (e.kind == Kind::ModelAdded)
                ++added;
            else if (e.kind == Kind::ModelRemoved)
                ++removed;
            else if (e.kind == Kind::ComponentRemoved)
                ++component_removed;
        });
    }
};
}

TEST_CASE("Session assembles subsystems and bridges model events", "[session]")
{
    RecordingObserver observer;
    Session s(&observer);
    EventCounter counter;
    counter.subscribe(s.events());

    SECTION("加模型：宿主观察者与 ModelEvent 同时收到通知")
    {
        const Index model_id = s.model().addModel("A", { });
        CHECK(observer.model_added == 1);
        CHECK(counter.added == 1);
        CHECK(s.query().hasModel(model_id));
    }

    SECTION("结构操作即时成 undo 记录，undo 恢复并通知")
    {
        s.model().addModel("A", { });
        REQUIRE(s.undoStack().canUndo());
        s.undoStack().undo();
        CHECK(s.query().listModels().empty());
        CHECK(observer.model_removed == 1);
        CHECK(counter.removed == 1);
        CHECK(s.undoStack().canRedo());
    }

    SECTION("removeModel 经会话入口删除并通知")
    {
        const Index model_id = s.model().addModel("A", { });
        s.removeModel(model_id);
        CHECK(s.query().listModels().empty());
        CHECK(observer.model_removed == 1);
        CHECK(counter.removed == 1);
    }

    SECTION("removeGeometry 为操作边界：undo 记录 + 通知 flush")
    {
        const Index model_id = s.model().addModel("A", { });
        auto geometry = std::make_unique<GeometryData>();
        geometry->setRootShape(GeometryBuilder::makeBox(0.0, 0.0, 0.0, 1.0, 1.0, 1.0));
        auto component = std::make_unique<ComponentData>();
        component->name = "Box";
        component->geometry = std::move(geometry);
        auto op = s.model().getModelOperator(model_id);
        const Index comp_id = op->addGeometryComponent(std::move(component));
        REQUIRE(s.query().hasComponent(comp_id));
        const int component_changed_before = observer.component_changed;

        s.removeGeometry(comp_id);
        CHECK(s.query().geometrySummary(comp_id).has_geometry == false);
        // removeGeometry 的边界 flush 发一次组件变更通知（addGeometryComponent 已即时通知过一次）
        CHECK(observer.component_changed == component_changed_before + 1);

        s.undoStack().undo();
        CHECK(s.query().geometrySummary(comp_id).has_geometry == true);
    }

    SECTION("teardown 幂等：重复调用与析构安全")
    {
        s.model().addModel("A", { });
        s.teardown();
        s.teardown();
    }
}

TEST_CASE("Session works without host observer", "[session]")
{
    Session s;
    EventCounter counter;
    counter.subscribe(s.events());

    s.model().addModel("A", { });
    CHECK(counter.added == 1);
    CHECK(s.query().listModels().size() == 1);
    CHECK(s.undoStack().canUndo());
}
