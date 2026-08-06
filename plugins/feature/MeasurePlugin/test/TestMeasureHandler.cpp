/**
 * @file TestMeasureHandler.cpp
 * @brief 测量处理器的单元测试
 * @author 范成通 email 1941804585@qq.com
 */

#include "MeasureHandler.h"
#include "EventBus.h"
#include "FeatureContext.h"
#include "FeatureEventGateway.h"
#include "FeatureEvents.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "InteractionContext.h"
#include "InteractionState.h"
#include "ModelLayer.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <optional>

using namespace systems::feature;

namespace {

//! @brief 功能测试环境：手工装配 FeatureContext（参数集 + 交互上下文），替代运行时注册流程
struct FeatureTestEnv {
    ModelLayer mgr;
    core::EventBus bus;
    FeatureEventGateway gateway { bus, mgr }; //> 事件网关（声明顺序须在 mgr/bus 之后）
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
            gateway,
            *params,
            interaction_ctx_,
            []() -> std::optional<Index> { return std::nullopt; },
            [this]() { return active_component; },
            [this](Index component_id) { return mgr.getComponentOperator(component_id); },
        });
    }
};

} // namespace

TEST_CASE("MeasureHandler setup declares clear button parameter and menu")
{
    MeasureHandler handler;
    FeatureRegistrar reg;
    handler.setup(reg);

    // 纯交互功能：仅"清除"按钮参数（无值触发器）与菜单项
    REQUIRE(reg.argTypes().size() == 1);
    CHECK(reg.argTypes()[0].type == ArgTypeEnum::Button);
    REQUIRE(reg.menuItems().size() == 1);
}

TEST_CASE("MeasureHandler: interactive picks update state annotations and ParameterChangedEvent clears")
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

    // 其他功能的参数变更不触发本功能清空（按功能名过滤，注册名见插件 json）
    env.bus.publish(ParameterChangedEvent { "OtherFeature", 0, core::ArgObject {} });
    CHECK_FALSE(env.interaction_state_.needs_refresh);
    CHECK_FALSE(env.interaction_state_.deferred_op);

    // 面板"清除"按钮参数：通过 ParameterChangedEvent 触发延迟清空（GUI 线程置位，渲染线程执行）
    env.bus.publish(ParameterChangedEvent { "MeasurePlugin", 0, core::ArgObject {} });
    // GUI 线程只置位延迟清空回调与刷新标志，不直接修改功能状态
    CHECK(env.interaction_state_.needs_refresh);
    REQUIRE(env.interaction_state_.deferred_op);
    // 模拟渲染线程 syncPending：执行延迟清空后检查状态
    env.interaction_state_.deferred_op();
    env.interaction_state_.deferred_op = nullptr;
    CHECK(env.handler.lineCount() == 0);
    CHECK(!env.handler.hasPending());
    CHECK(env.interaction_state_.annotations.lines.empty());
    CHECK(env.interaction_state_.annotations.points.empty());
    CHECK(env.interaction_state_.annotations.texts.empty());
}
