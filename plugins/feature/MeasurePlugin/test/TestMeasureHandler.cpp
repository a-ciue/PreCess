/**
 * @file TestMeasureHandler.cpp
 * @brief 测量处理器的单元测试
 * @author 范成通 email 1941804585@qq.com
 */

#include "EventBus.h"
#include "FeatureEvents.h"
#include "FeatureSystem.h"
#include "InteractionState.h"
#include "MeasureHandler.h"
#include "ModelLayer.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>

using namespace systems::feature;

namespace {

//! @brief 通过真实注册与激活装配上下文，测试只保留查询得到的非 owning 指针。
struct FeatureTestEnv {
    ModelLayer mgr;
    core::EventBus bus;
    FeatureSystem system { mgr, bus };
    MeasureHandler* handler;
    FeatureInfo* info;
    systems::interaction::InteractionState* interaction_state_;

    FeatureTestEnv()
    {
        auto owned = std::make_unique<MeasureHandler>();
        handler = owned.get();
        REQUIRE(system.registerHandler({ "MeasurePlugin", "测量", { }, true },
            FeatureSystem::SystemHandlerPtr { owned.release() }));
        info = system.getFeatureInfos().front();
        REQUIRE(system.setFeatureActive("MeasurePlugin"));
        interaction_state_ = system.activeInteraction();
        REQUIRE(interaction_state_);
        interaction_state_->needs_refresh = false; // 激活刷新已被渲染侧消费。
    }
};

} // namespace

TEST_CASE("MeasureHandler setup declares clear button parameter and menu")
{
    FeatureTestEnv env;

    // 纯交互功能：仅"清除"按钮参数（无值触发器）与菜单项
    REQUIRE(env.info->arg_types.size() == 1);
    CHECK(env.info->arg_types[0].type == ArgTypeEnum::Button);
    REQUIRE(env.info->navigation.entries().size() == 1);
}

TEST_CASE("MeasureHandler: interactive picks update state annotations and ParameterChangedEvent clears")
{
    FeatureTestEnv env;
    REQUIRE(env.interaction_state_->on_pick);

    // 两点成线：(0,0,0) → (1,0,0)
    systems::interaction::PickInfo p1;
    p1.valid = true;
    p1.world_pos = { 0.0, 0.0, 0.0 };
    p1.mesh_id = 0;
    systems::interaction::PickInfo p2;
    p2.valid = true;
    p2.world_pos = { 1.0, 0.0, 0.0 };
    p2.mesh_id = 1;

    env.interaction_state_->on_pick(p1);
    CHECK(env.handler->hasPending());
    env.interaction_state_->on_pick(p2);
    CHECK(env.handler->lineCount() == 1);

    // 交互标注写入 InteractionState.annotations（渲染层拉取绘制的契约）
    CHECK(env.interaction_state_->annotations.lines.size() == 1);
    CHECK(env.interaction_state_->annotations.points.size() == 2);
    CHECK(env.interaction_state_->annotations.texts.size() == 1);

    // 其他功能的参数变更被网关过滤，不触发本功能清空。
    env.bus.publish(ParameterChangedEvent { "OtherFeature", 0, core::ArgObject {} });
    CHECK_FALSE(env.interaction_state_->needs_refresh);
    CHECK_FALSE(env.interaction_state_->deferred_op);

    // 面板"清除"按钮参数：通过 ParameterChangedEvent 触发延迟清空（GUI 线程置位，渲染线程执行）
    env.bus.publish(ParameterChangedEvent { "MeasurePlugin", 0, core::ArgObject {} });
    // GUI 线程只置位延迟清空回调与刷新标志，不直接修改功能状态
    CHECK(env.interaction_state_->needs_refresh);
    REQUIRE(env.interaction_state_->deferred_op);
    // 模拟渲染线程 syncPending：执行延迟清空后检查状态
    env.interaction_state_->deferred_op();
    env.interaction_state_->deferred_op = nullptr;
    CHECK(env.handler->lineCount() == 0);
    CHECK(!env.handler->hasPending());
    CHECK(env.interaction_state_->annotations.lines.empty());
    CHECK(env.interaction_state_->annotations.points.empty());
    CHECK(env.interaction_state_->annotations.texts.empty());
}

TEST_CASE("MeasureHandler: deactivate schedules render-thread cleanup via deferRefresh")
{
    FeatureTestEnv env;

    // 先制造一条测量线
    systems::interaction::PickInfo p1;
    p1.valid = true;
    p1.world_pos = { 0.0, 0.0, 0.0 };
    p1.mesh_id = 0;
    systems::interaction::PickInfo p2;
    p2.valid = true;
    p2.world_pos = { 1.0, 0.0, 0.0 };
    p2.mesh_id = 1;
    env.interaction_state_->on_pick(p1);
    env.interaction_state_->on_pick(p2);
    REQUIRE(env.handler->lineCount() == 1);

    // 功能退出（GUI 线程）：不直接清状态，而是挂起渲染线程清理
    REQUIRE(env.system.setFeatureActive(""));
    CHECK(env.handler->lineCount() == 1); // GUI 线程不直接触碰交互状态
    CHECK(env.interaction_state_->needs_refresh);
    REQUIRE(env.interaction_state_->deferred_op);

    // 模拟渲染线程下线迁移消费 deferred_op（InteractionService::syncState 下线分支）
    env.interaction_state_->deferred_op();
    env.interaction_state_->deferred_op = nullptr;
    CHECK(env.handler->lineCount() == 0);
    CHECK(env.interaction_state_->annotations.lines.empty());
}
