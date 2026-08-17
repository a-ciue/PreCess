#include "EventBus.h"
#include "FeatureContext.h"
#include "FeatureEvents.h"
#include "FeatureHandler.h"
#include "FeatureRegistrar.h"
#include "FeatureSystem.h"
#include "InteractionState.h"
#include "ModelLayer.h"

#include <catch2/catch_test_macros.hpp>

using namespace systems::feature;

namespace {
/**
 * @brief 测试用假功能：记录各生命周期与回调的调用情况
 */
class FakeFeatureHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override
    {
        ++setup_count;
        reg.addParameter({ ArgTypeEnum::Float, "尺寸", "1.5", "网格尺寸" });
        reg.addParameter({ ArgTypeEnum::Int, "次数", "3", "" });
        reg.addMenuItem({ "工具", "假功能" });
        reg.addKeyBinding({ 'A', 0 });
        context = &ctx;
        // 订阅本功能的参数变更事件
        param_sub = ctx.events.subscribe<ParameterChangedEvent>(
            [this](const ParameterChangedEvent& e) {
                last_param_index = e.param_index;
                if (const auto* v = e.value.get<ArgTypeEnum::Float>()) {
                    last_float_value = *v;
                }
            });
    }
    void teardown(FeatureContext&) override
    {
        ++teardown_count;
        if (external_teardown_count) {
            ++*external_teardown_count; // 注销后 handler 已销毁，经外部计数器观测
        }
    }
    void activate(FeatureContext&) override
    {
        ++activate_count; // 功能进入（活动操作切换驱动）
    }
    void deactivate(FeatureContext&) override
    {
        ++deactivate_count; // 功能退出
        if (external_deactivate_count) {
            ++*external_deactivate_count; // 注销后 handler 已销毁，经外部计数器观测
        }
    }
    std::any execute(FeatureContext& ctx) override
    {
        ++execute_count;
        return 42;
    }
    bool onKeyEvent(const KeyEvent& event) override
    {
        ++key_event_count;
        last_key = event.key;
        return handle_key; // 由用例控制是否消费事件
    }

    int setup_count = 0;
    int teardown_count = 0;
    int activate_count = 0; //> 功能进入计数
    int deactivate_count = 0; //> 功能退出计数
    int execute_count = 0;
    int key_event_count = 0;
    int last_key = 0;
    bool handle_key = true; //> onKeyEvent 是否消费事件
    int* external_teardown_count = nullptr;
    int* external_deactivate_count = nullptr;
    FeatureContext* context = nullptr;
    core::EventBus::Subscription param_sub;
    std::size_t last_param_index = 999;
    double last_float_value = 0.0;
};

HandlerMetaData makeMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "FakeFeature";
    meta_data.display_name = "假功能";
    meta_data.description = "测试用功能";
    return meta_data;
}

}

TEST_CASE("FeatureSystem::registerHandler collects declarations and sets up", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem system(model_layer, bus);

    auto* raw = new FakeFeatureHandler;
    FeatureSystem::SystemHandlerPtr handler { raw };
    REQUIRE(system.registerHandler(makeMetaData(), std::move(handler)));

    REQUIRE(raw->setup_count == 1);
    REQUIRE(raw->context != nullptr);
    // 注册期不触发进入/退出回调：activate/deactivate 由活动操作切换驱动
    REQUIRE(raw->activate_count == 0);
    REQUIRE(raw->deactivate_count == 0);

    auto infos = system.getFeatureInfos();
    REQUIRE(infos.size() == 1);
    REQUIRE(infos[0]->name == "FakeFeature");
    REQUIRE(infos[0]->display_name == "假功能");
    REQUIRE(infos[0]->arg_types.size() == 2);
    REQUIRE(infos[0]->menus.size() == 1);
    REQUIRE(infos[0]->menus[0].menu_path == "工具");
    REQUIRE(infos[0]->key_bindings.size() == 1);
    REQUIRE(infos[0]->key_bindings[0].key == 'A');

    // 参数默认值取自 ArgType::content
    const FeatureParams* params = system.params("FakeFeature");
    REQUIRE(params != nullptr);
    REQUIRE(params->count() == 2);
    REQUIRE(*params->value(0).get<ArgTypeEnum::Float>() == 1.5);
    REQUIRE(*params->value(1).get<ArgTypeEnum::Int>() == 3);
}

TEST_CASE("FeatureSystem::setParameter updates value and publishes event", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem system(model_layer, bus);

    auto* raw = new FakeFeatureHandler;
    FeatureSystem::SystemHandlerPtr handler { raw };
    REQUIRE(system.registerHandler(makeMetaData(), std::move(handler)));

    REQUIRE(system.setParameter("FakeFeature", 0, core::ArgObject::create<ArgTypeEnum::Float>(2.5)));
    // 功能在 setup 中订阅了参数变更事件，实时收到新值
    REQUIRE(raw->last_param_index == 0);
    REQUIRE(raw->last_float_value == 2.5);
    REQUIRE(*system.params("FakeFeature")->value(0).get<ArgTypeEnum::Float>() == 2.5);

    REQUIRE_FALSE(system.setParameter("NoSuchFeature", 0, {}));
    REQUIRE_FALSE(system.setParameter("FakeFeature", 99, {}));
}

TEST_CASE("FeatureSystem::invoke dispatches to execute", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem system(model_layer, bus);

    auto* raw = new FakeFeatureHandler;
    FeatureSystem::SystemHandlerPtr handler { raw };
    REQUIRE(system.registerHandler(makeMetaData(), std::move(handler)));

    auto result = system.invoke("FakeFeature");
    REQUIRE(raw->execute_count == 1);
    REQUIRE(std::any_cast<int>(result) == 42);

    REQUIRE_FALSE(system.invoke("NoSuchFeature").has_value());
}

TEST_CASE("FeatureSystem routes KeyEvent to matched KeyBinding", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem system(model_layer, bus);

    auto* raw = new FakeFeatureHandler;
    FeatureSystem::SystemHandlerPtr handler { raw };
    REQUIRE(system.registerHandler(makeMetaData(), std::move(handler)));

    // 原始事件流经事件总线广播，订阅者总能收到（包括释放事件）
    int raw_event_count = 0;
    auto raw_sub = bus.subscribe<KeyEvent>([&](const KeyEvent&) { ++raw_event_count; });

    REQUIRE(system.dispatchKeyEvent(KeyEvent { 'A', 0, true })); // 命中绑定且功能消费
    REQUIRE(raw->key_event_count == 1);
    REQUIRE(raw->last_key == 'A');

    REQUIRE_FALSE(system.dispatchKeyEvent(KeyEvent { 'A', 0, false })); // 释放不触发绑定
    REQUIRE(raw->key_event_count == 1);

    REQUIRE_FALSE(system.dispatchKeyEvent(KeyEvent { 'B', 0, true })); // 键码不匹配
    REQUIRE(raw->key_event_count == 1);

    REQUIRE_FALSE(system.dispatchKeyEvent(KeyEvent { 'A', 1, true })); // 修饰键不匹配
    REQUIRE(raw->key_event_count == 1);

    // 功能选择不处理时，事件不被消费、继续传递
    raw->handle_key = false;
    REQUIRE_FALSE(system.dispatchKeyEvent(KeyEvent { 'A', 0, true }));
    REQUIRE(raw->key_event_count == 2);

    REQUIRE(raw_event_count == 5); // 所有派发的事件都经过了原始事件流
}

TEST_CASE("FeatureSystem context providers work when injected after registration", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem system(model_layer, bus);

    auto* raw = new FakeFeatureHandler;
    FeatureSystem::SystemHandlerPtr handler { raw };
    REQUIRE(system.registerHandler(makeMetaData(), std::move(handler)));

    // provider 在功能注册之后才注入，经系统转发依然生效
    system.setActiveModelProvider([]() { return std::optional<Index> { 7 }; });
    REQUIRE(raw->context->activeModel);
    REQUIRE(*raw->context->activeModel() == 7);
    REQUIRE(raw->context->activeComponent);
    REQUIRE_FALSE(raw->context->activeComponent().has_value());
}

TEST_CASE("FeatureSystem::unregisterHandler tears down and removes", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem system(model_layer, bus);

    int external_count = 0;
    auto* raw = new FakeFeatureHandler;
    raw->external_teardown_count = &external_count;
    FeatureSystem::SystemHandlerPtr handler { raw };
    REQUIRE(system.registerHandler(makeMetaData(), std::move(handler)));
    REQUIRE(system.getFeatureInfos().size() == 1);

    system.unregisterHandler(makeMetaData());
    REQUIRE(external_count == 1);
    REQUIRE(system.getFeatureInfos().empty());
    REQUIRE(system.params("FakeFeature") == nullptr);
}

TEST_CASE("FeatureSystem::unregisterHandler exits current feature before teardown", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem system(model_layer, bus);

    int external_teardown = 0;
    int external_deactivate = 0;
    auto* raw = new FakeFeatureHandler;
    raw->external_teardown_count = &external_teardown;
    raw->external_deactivate_count = &external_deactivate;
    FeatureSystem::SystemHandlerPtr handler { raw };
    REQUIRE(system.registerHandler(makeMetaData(), std::move(handler)));

    // 进入后注销当前功能：先 deactivate 退出，再 teardown（注销后 handler 已销毁，经外部计数器观测）
    REQUIRE(system.setFeatureActive("FakeFeature"));
    REQUIRE(raw->activate_count == 1);
    system.unregisterHandler(makeMetaData());
    REQUIRE(external_deactivate == 1);
    REQUIRE(external_teardown == 1);

    // 注销后当前功能已清空：空串退出为幂等空转
    REQUIRE(system.setFeatureActive(""));
}

TEST_CASE("FeatureSystem re-registering same name replaces old handler", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem system(model_layer, bus);

    int first_teardown_count = 0;
    auto* raw_first = new FakeFeatureHandler;
    raw_first->external_teardown_count = &first_teardown_count;
    FeatureSystem::SystemHandlerPtr first { raw_first };
    REQUIRE(system.registerHandler(makeMetaData(), std::move(first)));

    auto* raw_second = new FakeFeatureHandler;
    FeatureSystem::SystemHandlerPtr second { raw_second };
    REQUIRE(system.registerHandler(makeMetaData(), std::move(second)));

    REQUIRE(first_teardown_count == 1);
    REQUIRE(raw_second->setup_count == 1);
    REQUIRE(system.getFeatureInfos().size() == 1);
}

TEST_CASE("FeatureSystem propagates interactive metadata to FeatureInfo", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem system(model_layer, bus);

    auto meta_data = makeMetaData();
    meta_data.interactive = true;
    FeatureSystem::SystemHandlerPtr handler { new FakeFeatureHandler };
    REQUIRE(system.registerHandler(meta_data, std::move(handler)));

    auto infos = system.getFeatureInfos();
    REQUIRE(infos.size() == 1);
    REQUIRE(infos[0]->interactive);

    // 未声明时为 false
    FeatureSystem::SystemHandlerPtr plain { new FakeFeatureHandler };
    auto plain_meta = makeMetaData();
    plain_meta.name = "PlainFeature";
    REQUIRE(system.registerHandler(plain_meta, std::move(plain)));
    infos = system.getFeatureInfos();
    REQUIRE(infos.size() == 2);
    for (const FeatureInfo* info : infos) {
        if (info->name == "PlainFeature") {
            REQUIRE_FALSE(info->interactive);
        }
    }
}

TEST_CASE("FeatureSystem::activeInteraction tracks interactive activation", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem system(model_layer, bus);

    auto make_interactive = [](const std::string& name) {
        auto meta_data = makeMetaData();
        meta_data.name = name;
        meta_data.interactive = true;
        return meta_data;
    };

    auto* raw1 = new FakeFeatureHandler;
    FeatureSystem::SystemHandlerPtr first { raw1 };
    REQUIRE(system.registerHandler(make_interactive("FeatureA"), std::move(first)));
    auto* raw2 = new FakeFeatureHandler;
    FeatureSystem::SystemHandlerPtr second { raw2 };
    REQUIRE(system.registerHandler(make_interactive("FeatureB"), std::move(second)));

    // 初始无激活交互
    REQUIRE(system.activeInteraction() == nullptr);

    // 功能经 interaction 上下文激活自己后可被查到
    raw1->context->interaction.setActive(true);
    auto* active = system.activeInteraction();
    REQUIRE(active != nullptr);
    REQUIRE(active->active);

    // 单激活约定：第二个功能激活时，第一个被自动下线
    raw2->context->interaction.setActive(true);
    auto* active2 = system.activeInteraction();
    REQUIRE(active2 != nullptr);
    REQUIRE(active2 != active);
    REQUIRE_FALSE(active->active);

    // 取消激活后回到无激活状态
    raw2->context->interaction.setActive(false);
    REQUIRE(system.activeInteraction() == nullptr);
}

TEST_CASE("FeatureSystem::setFeatureActive drives feature enter/exit by name", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem system(model_layer, bus);

    auto interactive_meta = makeMetaData();
    interactive_meta.name = "InteractiveFeature";
    interactive_meta.interactive = true;
    auto* raw_interactive = new FakeFeatureHandler;
    FeatureSystem::SystemHandlerPtr interactive { raw_interactive };
    REQUIRE(system.registerHandler(interactive_meta, std::move(interactive)));

    auto plain_meta = makeMetaData();
    plain_meta.name = "PlainFeature";
    auto* raw_plain = new FakeFeatureHandler;
    FeatureSystem::SystemHandlerPtr plain { raw_plain };
    REQUIRE(system.registerHandler(plain_meta, std::move(plain)));

    // 未注册功能不可进入，且不改变现状
    REQUIRE_FALSE(system.setFeatureActive("Unknown"));
    REQUIRE(raw_plain->activate_count == 0);

    // 非 interactive 功能也可进入（进入/退出感知不限 interactive）
    REQUIRE(system.setFeatureActive("PlainFeature"));
    REQUIRE(raw_plain->activate_count == 1);
    REQUIRE(system.activeInteraction() == nullptr);

    // 幂等：同名重复设置无副作用
    REQUIRE(system.setFeatureActive("PlainFeature"));
    REQUIRE(raw_plain->activate_count == 1);

    // 切换：旧功能退出、新功能进入，interactive 的交互随之一并上线
    REQUIRE(system.setFeatureActive("InteractiveFeature"));
    REQUIRE(raw_plain->deactivate_count == 1);
    REQUIRE(raw_interactive->activate_count == 1);
    auto* active = system.activeInteraction();
    REQUIRE(active != nullptr);
    REQUIRE(active->active);

    // 空串退出当前功能，interactive 的交互随之下线
    REQUIRE(system.setFeatureActive(""));
    REQUIRE(raw_interactive->deactivate_count == 1);
    REQUIRE(system.activeInteraction() == nullptr);

    // 空串幂等空转
    REQUIRE(system.setFeatureActive(""));
    REQUIRE(raw_interactive->deactivate_count == 1);
}

TEST_CASE("InteractionContext::setActive notifies render refresh in both directions", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem system(model_layer, bus);

    auto meta = makeMetaData();
    meta.name = "NotifyTest";
    meta.interactive = true;
    auto* raw = new FakeFeatureHandler;
    FeatureSystem::SystemHandlerPtr handler { raw };
    REQUIRE(system.registerHandler(meta, std::move(handler)));

    int notify_count = 0;
    system.setRenderRefreshCallback([&notify_count] { ++notify_count; });

    // 激活：置位 needs_refresh 并 notify（渲染层 syncPending 经 syncState 上线）
    raw->context->interaction.setActive(true);
    auto* state = system.activeInteraction();
    REQUIRE(state != nullptr);
    CHECK(state->needs_refresh);
    CHECK(notify_count == 1);

    // 停用：同样置位 + notify（渲染层 syncPending 检测迁移并执行下线清理）
    state->needs_refresh = false;
    raw->context->interaction.setActive(false);
    CHECK_FALSE(state->active);
    CHECK(state->needs_refresh);
    CHECK(notify_count == 2);

    // 幂等守卫：重复调用不重复置位/通知
    state->needs_refresh = false;
    raw->context->interaction.setActive(false);
    CHECK_FALSE(state->needs_refresh);
    CHECK(notify_count == 2);

    // 合并语义：挂起刷新未消费时重复 requestRefresh 跳过 notify，消费后可再次 notify
    raw->context->interaction.requestRefresh();
    CHECK(notify_count == 3);
    raw->context->interaction.requestRefresh();
    CHECK(notify_count == 3);
    state->needs_refresh = false; // 模拟渲染层消费
    raw->context->interaction.requestRefresh();
    CHECK(notify_count == 4);
}
