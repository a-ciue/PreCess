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
    void setup(FeatureRegistrar& reg) override
    {
        ++setup_count;
        reg.addParameter({ ArgTypeEnum::Float, "尺寸", "1.5", "网格尺寸" });
        reg.addParameter({ ArgTypeEnum::Int, "次数", "3", "" });
        reg.addMenuItem({ "工具", "假功能" });
        reg.addKeyBinding({ 'A', 0 });
    }
    void activate(FeatureContext& ctx) override
    {
        ++activate_count;
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
    void deactivate() override
    {
        ++deactivate_count;
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
    int activate_count = 0;
    int deactivate_count = 0;
    int execute_count = 0;
    int key_event_count = 0;
    int last_key = 0;
    bool handle_key = true; //> onKeyEvent 是否消费事件
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

TEST_CASE("FeatureSystem::registerHandler collects declarations and activates", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem system(model_layer, bus);

    auto* raw = new FakeFeatureHandler;
    FeatureSystem::SystemHandlerPtr handler { raw };
    REQUIRE(system.registerHandler(makeMetaData(), std::move(handler)));

    REQUIRE(raw->setup_count == 1);
    REQUIRE(raw->activate_count == 1);
    REQUIRE(raw->context != nullptr);

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
    // 功能在 activate 中订阅了参数变更事件，实时收到新值
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

TEST_CASE("FeatureSystem::unregisterHandler deactivates and removes", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem system(model_layer, bus);

    int external_count = 0;
    auto* raw = new FakeFeatureHandler;
    raw->external_deactivate_count = &external_count;
    FeatureSystem::SystemHandlerPtr handler { raw };
    REQUIRE(system.registerHandler(makeMetaData(), std::move(handler)));
    REQUIRE(system.getFeatureInfos().size() == 1);

    system.unregisterHandler(makeMetaData());
    REQUIRE(external_count == 1);
    REQUIRE(system.getFeatureInfos().empty());
    REQUIRE(system.params("FakeFeature") == nullptr);
}

TEST_CASE("FeatureSystem re-registering same name replaces old handler", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem system(model_layer, bus);

    int first_deactivate_count = 0;
    auto* raw_first = new FakeFeatureHandler;
    raw_first->external_deactivate_count = &first_deactivate_count;
    FeatureSystem::SystemHandlerPtr first { raw_first };
    REQUIRE(system.registerHandler(makeMetaData(), std::move(first)));

    auto* raw_second = new FakeFeatureHandler;
    FeatureSystem::SystemHandlerPtr second { raw_second };
    REQUIRE(system.registerHandler(makeMetaData(), std::move(second)));

    REQUIRE(first_deactivate_count == 1);
    REQUIRE(raw_second->activate_count == 1);
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

TEST_CASE("FeatureSystem::setInteractionActive drives activation by feature name", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem system(model_layer, bus);

    auto interactive_meta = makeMetaData();
    interactive_meta.name = "InteractiveFeature";
    interactive_meta.interactive = true;
    FeatureSystem::SystemHandlerPtr interactive { new FakeFeatureHandler };
    REQUIRE(system.registerHandler(interactive_meta, std::move(interactive)));

    auto plain_meta = makeMetaData();
    plain_meta.name = "PlainFeature";
    FeatureSystem::SystemHandlerPtr plain { new FakeFeatureHandler };
    REQUIRE(system.registerHandler(plain_meta, std::move(plain)));

    // 未注册功能与未声明 interactive 的功能不可激活
    REQUIRE_FALSE(system.setInteractionActive("Unknown"));
    REQUIRE_FALSE(system.setInteractionActive("PlainFeature"));
    REQUIRE(system.activeInteraction() == nullptr);

    // 按名激活（幂等：重复设置无副作用）
    REQUIRE(system.setInteractionActive("InteractiveFeature"));
    REQUIRE(system.setInteractionActive("InteractiveFeature"));
    auto* active = system.activeInteraction();
    REQUIRE(active != nullptr);
    REQUIRE(active->active);

    // 活动操作切到无交互能力的功能：空串全部下线
    REQUIRE(system.setInteractionActive(""));
    REQUIRE(system.activeInteraction() == nullptr);
}
