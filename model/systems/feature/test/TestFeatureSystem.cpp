#include "AlgorithmHandler.h"
#include "AlgorithmSystem.h"
#include "ComponentData.h"
#include "EventBus.h"
#include "FeatureContext.h"
#include "FeatureEvents.h"
#include "FeatureHandler.h"
#include "FeatureRegistrar.h"
#include "FeatureSystem.h"
#include "InteractionState.h"
#include "Job.h"
#include "JobRunner.h"
#include "MeshData.h"
#include "ModelData.h"
#include "ModelIOSystem.h"
#include "ModelLayer.h"
#include "UndoStack.h"
#include "test/OwnerQueue.h"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

using namespace systems::feature;
using namespace std::chrono_literals;

namespace {
// worker 只投递，测试线程负责 GUI 提交；真实模型测试不豁免线程亲和。
std::mutex gui_queue_mutex;
std::vector<std::function<void()>> gui_queue;

void dispatchGui(std::function<void()> fn)
{
    std::lock_guard lock(gui_queue_mutex);
    gui_queue.push_back(std::move(fn));
}

template <class Duration, class Predicate>
bool waitForGui(std::unique_lock<std::mutex>& lock, std::condition_variable& cv,
    Duration timeout, Predicate predicate)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!predicate()) {
        lock.unlock();
        std::vector<std::function<void()>> pending;
        {
            std::lock_guard queue_lock(gui_queue_mutex);
            pending.swap(gui_queue);
        }
        for (auto& fn : pending)
            fn();
        lock.lock();
        if (std::chrono::steady_clock::now() >= deadline)
            return predicate();
        cv.wait_for(lock, 1ms, predicate);
    }
    return true;
}

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

    REQUIRE_FALSE(system.setParameter("NoSuchFeature", 0, { }));
    REQUIRE_FALSE(system.setParameter("FakeFeature", 99, { }));
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

    // 固定服务始终可调用；无 provider、无 undo 栈时返回空或空转。
    REQUIRE_FALSE(raw->context->activeModel());
    REQUIRE_FALSE(raw->context->componentOperator(-1));
    REQUIRE_FALSE(raw->context->undo.beginScope("无栈预览"));
    REQUIRE_FALSE(raw->context->undo.scopeActive());
    REQUIRE_NOTHROW(raw->context->undo.cancelScope());
    REQUIRE_NOTHROW(raw->context->undo.revertScope());
    // provider 在功能注册之后才注入，经系统查询依然生效。
    system.setActiveModelProvider([]() { return std::optional<Index> { 7 }; });
    REQUIRE_FALSE(raw->context->activeComponent().has_value());
    REQUIRE(*raw->context->activeModel() == 7);
    system.setActiveComponentProvider([] { return std::optional<Index> { 9 }; });
    REQUIRE(raw->context->activeComponent() == 9);
    system.setActiveComponentProvider({ });
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

namespace {
/**
 * @brief 测试用自由任务功能：execute 经 ctx.runJob 提交 undo-free 任务
 */
class JobSubmittingFeature : public FeatureHandler {
public:
    std::any execute(FeatureContext& ctx) override
    {
        last_job = ctx.runJob("free-task",
            [](systems::job::ProgressFn report) {
                report(0.5, "half");
            });
        return { };
    }
    std::shared_ptr<systems::job::Job> last_job;
};

HandlerMetaData makeJobMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "JobFeature";
    meta_data.display_name = "任务功能";
    meta_data.description = "自由任务测试";
    return meta_data;
}
}

TEST_CASE("FeatureSystem free job runs with progress and finish notifications", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem system(model_layer, bus);
    systems::job::JobRunner runner(model_layer, nullptr, dispatchGui);
    system.setJobRunner(&runner);

    std::mutex mutex;
    std::condition_variable cv;
    int started_count = 0;
    int finished_count = 0;
    double last_progress = -1.0;
    std::string last_label;
    // started 在提交线程（本线程）同步触发；progress/finished 在工作线程触发
    runner.setOnStarted([&](systems::job::Job& job) {
        REQUIRE(job.state() == systems::job::JobState::Running);
        ++started_count;
    });
    runner.setOnProgress([&](systems::job::Job&, double value, const std::string& label) {
        std::lock_guard lock(mutex);
        last_progress = value;
        last_label = label;
    });
    runner.setOnFinished([&](systems::job::Job& job) {
        std::lock_guard lock(mutex);
        ++finished_count;
        cv.notify_all();
    });

    auto* raw = new JobSubmittingFeature;
    REQUIRE(system.registerHandler(makeJobMetaData(), FeatureSystem::SystemHandlerPtr { raw }));

    system.invoke("JobFeature");
    REQUIRE(raw->last_job != nullptr); // 槽空闲：提交成功
    REQUIRE(started_count == 1);

    std::unique_lock lock(mutex);
    const bool done = waitForGui(lock, cv, 5s, [&] { return finished_count == 1; });
    REQUIRE(done);
    REQUIRE(last_progress == 0.5); // finished（工作线程）先于本等待建立 happens-before
    REQUIRE(last_label == "half");
    lock.unlock();
    REQUIRE(raw->last_job->state() == systems::job::JobState::Done);
}

TEST_CASE("FeatureSystem free job dropped while slot occupied", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model;
    FeatureSystem system(model, bus);
    OwnerQueue queue;
    systems::job::JobRunner runner(model, nullptr, queue.dispatcher());
    system.setJobRunner(&runner);
    auto handler = std::make_unique<JobSubmittingFeature>();
    auto* raw = handler.get();
    REQUIRE(system.registerHandler(makeJobMetaData(), FeatureSystem::SystemHandlerPtr { handler.release() }));
    auto blocker = runner.run("blocker", [] { return systems::job::JobWork { [](systems::job::ProgressFn) { } }; });
    REQUIRE(blocker);
    auto complete = queue.take();
    system.invoke("JobFeature");
    REQUIRE_FALSE(raw->last_job); // 忙时命令拒绝，不排队。
    REQUIRE(runner.currentJob() == blocker);
    complete();
    REQUIRE(blocker->state() == systems::job::JobState::Done);
}

TEST_CASE("FeatureSystem free job returns empty without runner", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem system(model_layer, bus); // 未注入执行器

    auto* raw = new JobSubmittingFeature;
    REQUIRE(system.registerHandler(makeJobMetaData(), FeatureSystem::SystemHandlerPtr { raw }));

    system.invoke("JobFeature");
    REQUIRE(raw->last_job == nullptr);
}

namespace {
//! 构造简单三角形组件并入池（冻结任务测试的目标）
Index addTriangleComponent(ModelLayer& mgr, const std::string& name = "Comp_0")
{
    auto mesh = std::make_unique<MeshData>();
    mesh->init();
    mesh->vertex_positions_ = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
    mesh->face_vertices_ = { 0, 1, 2 };
    mesh->face_vertices_offset_ = { 0, 3 };

    auto c = std::make_unique<ComponentData>();
    c->name = name;
    c->mesh = std::move(mesh);
    ComponentDatas comps;
    comps.push_back(std::move(c));

    const Index model_id = mgr.addModel(name + "_model", std::move(comps));
    return mgr.modelById(model_id)->componentIds()[0];
}

std::size_t pointCount(ModelLayer& mgr, Index component_id)
{
    auto op = mgr.getComponentOperator(component_id);
    REQUIRE(op.has_value());
    return op->component().mesh->vertex_positions_.size();
}

/**
 * @brief 测试用冻结任务功能：execute 经 ctx.runModelJob 提交，任务体由测试注入
 */
class ModelJobFeature : public FeatureHandler {
public:
    void setup(FeatureRegistrar&, FeatureContext& ctx) override { context = &ctx; }
    FeatureContext* context { nullptr };
    std::any execute(FeatureContext& ctx) override
    {
        // 拷贝而非 move：handler 可被多次 invoke（move 会掏空 task，二次 invoke 撞空 function）
        last_job = ctx.runModelJob("Append", target_component, task);
        return { };
    }
    Index target_component { -1 };
    ModelJobTaskFn task;
    std::shared_ptr<systems::job::Job> last_job;
};

HandlerMetaData makeModelJobMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "ModelJobFeature";
    meta_data.display_name = "冻结任务功能";
    meta_data.description = "冻结任务测试";
    return meta_data;
}
}

TEST_CASE("FeatureSystem model job commits shadow mutation with undo", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer); // 先建组件再挂 undo：结构入栈不计入断言
    UndoStack undo(model_layer);
    model_layer.setUndoRecorder(&undo);
    FeatureSystem system(model_layer, bus, &undo);
    systems::job::JobRunner runner(model_layer, &undo, dispatchGui); // 就地分发（测试无事件循环）
    system.setJobRunner(&runner);

    std::mutex mutex;
    std::condition_variable cv;
    int started_count = 0;
    int finished_count = 0;
    bool started_as_model_job = false;
    bool finished_as_model_job = false;
    runner.setOnStarted([&](systems::job::Job& job) {
        const bool model_job = job.masked();
        ++started_count;
        started_as_model_job = model_job;
    });
    runner.setOnFinished([&](systems::job::Job& job) {
        const bool model_job = job.masked();
        std::lock_guard lock(mutex);
        ++finished_count;
        finished_as_model_job = model_job;
        cv.notify_all();
    });

    auto* raw = new ModelJobFeature;
    raw->target_component = comp;
    raw->task = [](ComponentOperator& shadow_target, systems::job::ProgressFn report) {
        shadow_target.appendPoint({ 9.0, 9.0, 9.0 });
        report(1.0, "done");
    };
    REQUIRE(system.registerHandler(makeModelJobMetaData(), FeatureSystem::SystemHandlerPtr { raw }));

    system.invoke("ModelJobFeature");
    REQUIRE(raw->last_job != nullptr);
    REQUIRE(started_count == 1);
    REQUIRE(started_as_model_job); // 冻结类别标记（UI 遮罩源）

    std::unique_lock lock(mutex);
    REQUIRE(waitForGui(lock, cv, 5s, [&] { return finished_count == 1; }));
    lock.unlock();
    REQUIRE(finished_as_model_job);
    REQUIRE(raw->last_job->state() == systems::job::JobState::Done);

    // 提交后真实模型落影子变更 + undo 一条记录；undo 回滚（写前标脏 before-image）
    REQUIRE(pointCount(model_layer, comp) == 4);
    REQUIRE(undo.canUndo());
    REQUIRE_NOTHROW(undo.undo());
    REQUIRE(pointCount(model_layer, comp) == 3);
}

TEST_CASE("FeatureSystem model job failure is atomic", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer); // 先建组件再挂 undo：结构入栈不计入断言
    UndoStack undo(model_layer);
    model_layer.setUndoRecorder(&undo);
    FeatureSystem system(model_layer, bus, &undo);
    systems::job::JobRunner runner(model_layer, &undo, dispatchGui);
    system.setJobRunner(&runner);

    std::mutex mutex;
    std::condition_variable cv;
    int finished_count = 0;
    runner.setOnFinished([&](systems::job::Job& job) {
        std::lock_guard lock(mutex);
        ++finished_count;
        cv.notify_all();
    });

    auto* raw = new ModelJobFeature;
    raw->target_component = comp;
    raw->task = [](ComponentOperator& shadow_target, systems::job::ProgressFn) -> void {
        shadow_target.appendPoint({ 9.0, 9.0, 9.0 });
        throw std::runtime_error("boom"); // 影子已写、任务失败
    };
    REQUIRE(system.registerHandler(makeModelJobMetaData(), FeatureSystem::SystemHandlerPtr { raw }));

    system.invoke("ModelJobFeature");
    REQUIRE(raw->last_job != nullptr);

    std::unique_lock lock(mutex);
    REQUIRE(waitForGui(lock, cv, 5s, [&] { return finished_count == 1; }));
    lock.unlock();
    REQUIRE(raw->last_job->state() == systems::job::JobState::Failed);

    // 失败原子性：影子丢弃——真实模型零变化、undo 零记录
    REQUIRE(pointCount(model_layer, comp) == 3);
    REQUIRE_FALSE(undo.canUndo());
}

TEST_CASE("FeatureSystem model job cancel discards shadow at heartbeat", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer); // 先建组件再挂 undo：结构入栈不计入断言
    UndoStack undo(model_layer);
    model_layer.setUndoRecorder(&undo);
    FeatureSystem system(model_layer, bus, &undo);
    systems::job::JobRunner runner(model_layer, &undo, dispatchGui);
    system.setJobRunner(&runner);

    std::mutex mutex;
    std::condition_variable cv;
    int finished_count = 0;
    runner.setOnFinished([&](systems::job::Job& job) {
        std::lock_guard lock(mutex);
        ++finished_count;
        cv.notify_all();
    });

    auto* raw = new ModelJobFeature;
    raw->target_component = comp;
    raw->task = [](ComponentOperator& shadow_target, systems::job::ProgressFn report) {
        // 心跳循环直至取消（上限防挂）；取消后由 runner 包裹回调抛出，写点不达
        for (int i = 0; i < 400; ++i) {
            report(0.5, "working");
            std::this_thread::sleep_for(5ms);
        }
        shadow_target.appendPoint({ 9.0, 9.0, 9.0 });
    };
    REQUIRE(system.registerHandler(makeModelJobMetaData(), FeatureSystem::SystemHandlerPtr { raw }));

    system.invoke("ModelJobFeature");
    REQUIRE(raw->last_job != nullptr);
    raw->last_job->cancel(); // 立即请求取消：任务体下次心跳感知

    std::unique_lock lock(mutex);
    REQUIRE(waitForGui(lock, cv, 5s, [&] { return finished_count == 1; }));
    lock.unlock();
    REQUIRE(raw->last_job->state() == systems::job::JobState::Cancelled);

    // 取消 = 影子丢弃：真实模型零变化、undo 零记录
    REQUIRE(pointCount(model_layer, comp) == 3);
    REQUIRE_FALSE(undo.canUndo());
}

TEST_CASE("FeatureSystem model job rejections", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer); // 先建组件再挂 undo：结构入栈不计入断言
    UndoStack undo(model_layer);
    model_layer.setUndoRecorder(&undo);
    FeatureSystem system(model_layer, bus, &undo);
    // 不注入执行器：资格拒绝之一
    auto* raw = new ModelJobFeature;
    raw->target_component = comp;
    raw->task = [](ComponentOperator&, systems::job::ProgressFn) { };
    REQUIRE(system.registerHandler(makeModelJobMetaData(), FeatureSystem::SystemHandlerPtr { raw }));

    // 无执行器 → 拒绝
    system.invoke("ModelJobFeature");
    REQUIRE(raw->last_job == nullptr);

    // 目标组件不存在 → 拒绝
    systems::job::JobRunner runner(model_layer, &undo, dispatchGui);
    system.setJobRunner(&runner);
    raw->target_component = 99999;
    system.invoke("ModelJobFeature");
    REQUIRE(raw->last_job == nullptr);

    // 层会话进行中 → 拒绝（写路径互斥，防隐式 cancel 层）。层须与 invoke 同所有者——
    // 否则 invoke 的边界按"边界抢占"先撤销它（未闭合的预览让位给别人），拒绝就不成立了
    raw->target_component = comp;
    {
        UndoStack::OwnerScope owner(&undo, "ModelJobFeature");
        REQUIRE(undo.beginScope("preview"));
    }
    system.invoke("ModelJobFeature");
    REQUIRE(raw->last_job == nullptr);
    undo.cancelScope();
}

namespace {
//! 闸测试事件（泛型总线，任意可拷贝类型可作事件）
struct GateTestEvent {
    int payload { 0 };
};

/**
 * @brief 测试用闸功能：execute 发冻结任务；setup 经 ctx.events 订阅事件；按键回调计数
 */
class GateFeature : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override
    {
        context = &ctx;
        event_sub = ctx.events.subscribe<ModelEvent>([this](const ModelEvent&) { ++event_ran; });
        command_sub = ctx.events.subscribe<GateTestEvent>([this](const GateTestEvent&) {
            ++command_ran;
            context->model.getComponentOperator(target)->appendPoint({ 9, 9, 9 });
        });
        reg.addMenuItem({ "测试", "闸功能" });
        reg.addKeyBinding({ 'A', 0 }); // 无绑定则 dispatchKeyEvent 恒 false，按键断言为空测
    }
    std::any execute(FeatureContext& ctx) override
    {
        last_job = ctx.runModelJob("Freeze", target, std::move(task));
        return { };
    }
    bool onKeyEvent(const KeyEvent&) override
    {
        ++key_ran;
        return true;
    }

    FeatureContext* context { nullptr };
    Index target { -1 };
    ModelJobTaskFn task;
    std::shared_ptr<systems::job::Job> last_job;
    core::EventBus::Subscription event_sub;
    int event_ran { 0 };
    int key_ran { 0 };
    core::EventBus::Subscription command_sub;
    int command_ran { 0 };
};

HandlerMetaData makeGateMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "GateFeature";
    meta_data.display_name = "闸功能";
    meta_data.description = "冻结闸测试";
    return meta_data;
}
}

TEST_CASE("FeatureSystem defers model notifications and rejects keys during model job", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer); // 先建组件再挂 undo：结构入栈不计入断言
    UndoStack undo(model_layer);
    model_layer.setUndoRecorder(&undo);
    FeatureSystem system(model_layer, bus, &undo);
    systems::job::JobRunner runner(model_layer, &undo, dispatchGui); // 就地分发
    system.setJobRunner(&runner);

    std::mutex mutex;
    std::condition_variable cv;
    bool release = false;
    int finished_count = 0;
    runner.setOnFinished([&](systems::job::Job& job) {
        std::lock_guard lock(mutex);
        ++finished_count;
        cv.notify_all();
    });

    auto* raw = new GateFeature;
    raw->target = comp;
    raw->task = [&](ComponentOperator&, systems::job::ProgressFn) {
        std::unique_lock lock(mutex);
        cv.wait(lock, [&] { return release; }); // 阻塞维持冻结窗口
    };
    REQUIRE(system.registerHandler(makeGateMetaData(), FeatureSystem::SystemHandlerPtr { raw }));

    system.invoke("GateFeature");
    REQUIRE(raw->last_job != nullptr); // 冻结窗口已开

    // 冻结中：事件回调整体延后、按键派发整体拒绝
    bus.publish(ModelEvent { });
    REQUIRE(raw->event_ran == 0);
    bus.publish(GateTestEvent { }); // 用户命令在写入前拒绝，不留作解锁后的重放。
    CHECK(raw->command_ran == 1);
    CHECK(pointCount(model_layer, comp) == 3);
    REQUIRE_FALSE(system.dispatchKeyEvent(KeyEvent { 'A', 0, true }));
    REQUIRE(raw->key_ran == 0);

    // 解除阻塞至任务终态（提交必先完成——runner 先 commit 后 finish）
    {
        std::lock_guard lock(mutex);
        release = true;
    }
    cv.notify_all();
    {
        std::unique_lock lock(mutex);
        REQUIRE(waitForGui(lock, cv, 5s, [&] { return finished_count == 1; }));
    }
    REQUIRE(raw->last_job->state() == systems::job::JobState::Done);
    REQUIRE(raw->event_ran == 0); // 独立系统宿主显式消费，Session 配置自动消费

    // app 层在终态后于 GUI 线程重放（此处主线程代行）
    system.flushDeferredEvents();
    REQUIRE(raw->event_ran == 1);
    REQUIRE(raw->command_ran == 1);
    REQUIRE(pointCount(model_layer, comp) == 3);

    // 解冻后：新事件直通、按键恢复
    bus.publish(ModelEvent { });
    REQUIRE(raw->event_ran == 2);
    REQUIRE(system.dispatchKeyEvent(KeyEvent { 'A', 0, true }));
    REQUIRE(raw->key_ran == 1);
}

namespace {
/**
 * @brief 测试用事件发任务功能：事件回调内经 ctx 发自由任务
 */
class EventJobFeature : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override
    {
        context = &ctx;
        event_sub = ctx.events.subscribe<GateTestEvent>([this](const GateTestEvent&) {
            last_free_job = context->runJob("free-from-event",
                [](systems::job::ProgressFn report) {
                    report(1.0, "done");
                });
        });
        reg.addMenuItem({ "测试", "事件发任务" });
    }

    FeatureContext* context { nullptr };
    std::shared_ptr<systems::job::Job> last_free_job;
    core::EventBus::Subscription event_sub;
};

HandlerMetaData makeEventJobMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "EventJobFeature";
    meta_data.display_name = "事件发任务";
    meta_data.description = "事件路径发 job 测试";
    return meta_data;
}
}

TEST_CASE("FeatureSystem event callback publishes free job", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem system(model_layer, bus);
    systems::job::JobRunner runner(model_layer, nullptr, dispatchGui);
    system.setJobRunner(&runner);

    std::mutex mutex;
    std::condition_variable cv;
    int finished_count = 0;
    runner.setOnFinished([&](systems::job::Job& job) {
        std::lock_guard lock(mutex);
        ++finished_count;
        cv.notify_all();
    });

    auto* raw = new EventJobFeature;
    REQUIRE(system.registerHandler(makeEventJobMetaData(), FeatureSystem::SystemHandlerPtr { raw }));

    bus.publish(GateTestEvent { }); // 非冻结期：回调直通并同步提交
    REQUIRE(raw->last_free_job != nullptr);

    std::unique_lock lock(mutex);
    REQUIRE(waitForGui(lock, cv, 5s, [&] { return finished_count == 1; }));
    lock.unlock();
    REQUIRE(raw->last_free_job->state() == systems::job::JobState::Done);
}

// —— 写闸（精确收权到 undo 语义）——

TEST_CASE("Real model writes from worker threads are rejected by write gate", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer);
    FeatureSystem system(model_layer, bus);
    systems::job::JobRunner runner(model_layer, nullptr, dispatchGui);
    system.setJobRunner(&runner);

    // 闭包洞复现：任务体在提交线程（GUI）拿到真实组件写面，移交后在工作线程使用——
    // 自由任务"无写面"的签名约束本挡不住闭包捕获，写闸是执行点
    auto op = model_layer.getComponentOperator(comp);
    REQUIRE(op.has_value());
    const std::size_t before = pointCount(model_layer, comp);

    std::mutex mutex;
    std::condition_variable cv;
    bool finished = false;
    runner.setOnFinished([&](systems::job::Job&) {
            std::lock_guard lock(mutex);
            finished = true;
            cv.notify_all(); });
    auto job = runner.run("offthread-write", [&] { return systems::job::JobWork { [op](systems::job::ProgressFn) mutable -> void {
            op->appendPoint({ 9.0, 9.0, 9.0 }); // 工作线程写真实层 → 写闸拒绝（写前标脏先于数据修改）
            return; }, { } }; }, { });
    REQUIRE(job != nullptr);

    std::unique_lock lock(mutex);
    REQUIRE(waitForGui(lock, cv, 5s, [&] { return finished; }));
    lock.unlock();

    REQUIRE(job->state() == systems::job::JobState::Failed);
    REQUIRE(job->error().find("off-thread real-model write rejected") != std::string::npos);
    REQUIRE(pointCount(model_layer, comp) == before); // 越权写零残留
}

TEST_CASE("Real model writes are rejected during frozen snapshot window", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer);
    UndoStack undo(model_layer);
    model_layer.setUndoRecorder(&undo); // 先建组件再挂 undo：结构入栈不污染断言
    FeatureSystem system(model_layer, bus, &undo);
    systems::job::JobRunner runner(model_layer, &undo, dispatchGui);
    system.setJobRunner(&runner);

    std::mutex mutex;
    std::condition_variable cv;
    bool started = false;
    bool release = false;
    bool finished = false;

    auto* raw = new ModelJobFeature;
    raw->target_component = comp;
    raw->task = [&](ComponentOperator&, systems::job::ProgressFn) -> void {
        {
            std::lock_guard lock(mutex);
            started = true;
        }
        cv.notify_all();
        std::unique_lock lock(mutex);
        cv.wait(lock, [&] { return release; });
        return;
    };
    REQUIRE(system.registerHandler(makeModelJobMetaData(), FeatureSystem::SystemHandlerPtr { raw }));
    runner.setOnFinished([&](systems::job::Job& job) {
        std::lock_guard lock(mutex);
        finished = true;
        cv.notify_all();
    });

    system.invoke("ModelJobFeature");
    REQUIRE(raw->last_job != nullptr); // 槽空闲：提交成功

    {
        std::unique_lock lock(mutex);
        REQUIRE(waitForGui(lock, cv, 5s, [&] { return started; })); // 任务体已在工作线程开跑
    }
    REQUIRE(model_layer.writesFrozen()); // copy-in 成立：窗口开启

    // GUI 线程在窗口期写真实层 → 拒（框架特权不在手上）：组件级与结构级同闸
    auto op = model_layer.getComponentOperator(comp);
    REQUIRE(op.has_value());
    auto throwsFrozen = [](const std::function<void()>& write) {
        try {
            write();
        } catch (const std::runtime_error& e) {
            return dynamic_cast<const ModelOperationBusy*>(&e) != nullptr;
        }
        return false;
    };
    REQUIRE(throwsFrozen([&] { op->appendPoint({ 5.0, 5.0, 5.0 }); }));
    REQUIRE(throwsFrozen([&] { model_layer.removeComponent(comp); }));
    REQUIRE(pointCount(model_layer, comp) == 3); // 拒绝零残留

    {
        std::lock_guard lock(mutex);
        release = true;
    }
    cv.notify_all();
    {
        std::unique_lock lock(mutex);
        REQUIRE(waitForGui(lock, cv, 5s, [&] { return finished; }));
    }
    REQUIRE_FALSE(model_layer.writesFrozen()); // 提交段（特权）与终态先后关窗
    REQUIRE(pointCount(model_layer, comp) == 3); // 任务体未写影子 → 提交空操作

    // 窗口关闭：GUI 写恢复（写在边界内——恢复的是写闸，写入本身须有记账归属）
    op = model_layer.getComponentOperator(comp);
    undo.beginOperation("窗口关闭后写");
    REQUIRE_NOTHROW(op->appendPoint({ 7.0, 7.0, 7.0 }));
    undo.commitOperation();
    REQUIRE(pointCount(model_layer, comp) == 4);
}

// —— 类型化终态回写（写模型合法通道）与 undo/redo 任务联动 ——

namespace {
//! 注入式任务体的自由任务功能（回写与联动测试共用）
class WritebackJobFeature : public FeatureHandler {
public:
    std::any execute(FeatureContext& ctx) override
    {
        if (write)
            last_job = ctx.runTypedWriteback("回写任务", target, [task = std::move(task)](systems::job::ProgressFn report) { task(report); return 0; }, [write = std::move(write)](ComponentOperator& op, int&) { write(op); });
        else
            last_job = ctx.runJob("回写任务", std::move(task));
        return { };
    }
    Index target { -1 };
    WritebackFn write;
    systems::job::JobTaskFn task;
    std::shared_ptr<systems::job::Job> last_job;
};

HandlerMetaData makeWritebackMeta()
{
    HandlerMetaData meta_data;
    meta_data.name = "WritebackJobFeature";
    meta_data.display_name = "回写任务功能";
    meta_data.description = "回写/联动测试";
    return meta_data;
}
}

TEST_CASE("GUI model writes rejected while free job in flight; reads stay open", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer);
    FeatureSystem system(model_layer, bus);
    systems::job::JobRunner runner(model_layer, nullptr, dispatchGui);
    system.setJobRunner(&runner);

    // 任务在飞期（提交至终态）：软冻结窗只锁碰模型数据的写，不碰模型数据的操作照常
    std::mutex mutex;
    std::condition_variable cv;
    bool started = false;
    bool release = false;
    bool finished = false;
    auto* raw = new WritebackJobFeature;
    raw->task = [&](systems::job::ProgressFn) -> void {
        {
            std::lock_guard lock(mutex);
            started = true;
        }
        cv.notify_all();
        std::unique_lock lock(mutex);
        cv.wait(lock, [&] { return release; });
        return;
    };
    REQUIRE(system.registerHandler(makeWritebackMeta(), FeatureSystem::SystemHandlerPtr { raw }));
    runner.setOnFinished([&](systems::job::Job& job) {
        std::lock_guard lock(mutex);
        finished = true;
        cv.notify_all();
    });

    system.invoke("WritebackJobFeature");
    REQUIRE(raw->last_job != nullptr);
    {
        std::unique_lock lock(mutex);
        REQUIRE(waitForGui(lock, cv, 5s, [&] { return started; }));
    }
    REQUIRE_FALSE(model_layer.writesFrozen()); // 自由任务不开冻结窗（软冻结是写闸窗，非遮罩）
    REQUIRE(model_layer.writesPending()); // 提交即开软冻结窗

    // 区分一：碰模型数据 → 拒（写前拦截，数据零变化）
    {
        auto op = model_layer.getComponentOperator(comp);
        REQUIRE(op.has_value()); // 断言移出 try：Catch 断言异常不得被本用例 catch 吞掉
        std::string caught;
        try {
            op->appendPoint({ 4.0, 4.0, 4.0 });
        } catch (const std::exception& e) {
            caught = e.what();
        }
        REQUIRE(caught.find("writeback is pending") != std::string::npos);
        REQUIRE(pointCount(model_layer, comp) == 3);
    }
    // 区分二：不碰模型数据的操作 → 放行（只读查询不经写闸）
    REQUIRE(pointCount(model_layer, comp) == 3);
    REQUIRE(model_layer.getComponentOperator(comp).has_value());

    // 终态 → 关窗 → 写恢复
    {
        std::lock_guard lock(mutex);
        release = true;
    }
    cv.notify_all();
    {
        std::unique_lock lock(mutex);
        REQUIRE(waitForGui(lock, cv, 5s, [&] { return finished; }));
    }
    REQUIRE(raw->last_job->state() == systems::job::JobState::Done);
    REQUIRE_FALSE(model_layer.writesPending());
    auto after = model_layer.getComponentOperator(comp);
    REQUIRE(after.has_value());
    REQUIRE_NOTHROW(after->appendPoint({ 4.0, 4.0, 4.0 })); // 本 fixture 无记账器：写归属判定不激活
    REQUIRE(pointCount(model_layer, comp) == 4);
}

TEST_CASE("Free job writeback applies on caller thread inside undo boundary", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer);
    UndoStack undo(model_layer);
    model_layer.setUndoRecorder(&undo);
    FeatureSystem system(model_layer, bus, &undo);

    // 分发器排队、主测试线程执行提交段（模拟生产中 GUI 执行回写——写闸线程亲和依赖此点）
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<std::function<void()>> queued;
    bool finished = false;
    systems::job::JobRunner runner(model_layer, &undo, [&](std::function<void()> fn) {
        std::lock_guard lock(mutex);
        queued.push_back(std::move(fn));
        cv.notify_all();
    });
    system.setJobRunner(&runner);
    runner.setOnFinished([&](systems::job::Job& job) {
        std::lock_guard lock(mutex);
        finished = true;
        cv.notify_all();
    });

    auto* raw = new WritebackJobFeature;
    ModelLayer* model = &model_layer;
    bool apply_ran = false;
    raw->target = comp;
    raw->write = [model, comp, &apply_ran](ComponentOperator&) {
        apply_ran = true;
        auto op = model->getComponentOperator(comp); // 回写段内防御式查找（目标可能已被删）
        if (!op || !op->component().mesh)
            return;
        const auto n = op->component().mesh->vertex_positions_.size();
        op->appendPoint({ static_cast<double>(n), 8.0, 8.0 });
    };
    raw->task = [model, comp, &apply_ran](systems::job::ProgressFn) {
    };
    REQUIRE(system.registerHandler(makeWritebackMeta(), FeatureSystem::SystemHandlerPtr { raw }));

    system.invoke("WritebackJobFeature");
    REQUIRE(raw->last_job != nullptr);

    // 等提交闭包入队 → 主线程执行（= 生产中的 GUI 回写段）
    std::function<void()> commit_fn;
    {
        std::unique_lock lock(mutex);
        REQUIRE(waitForGui(lock, cv, 5s, [&] { return !queued.empty(); }));
        commit_fn = std::move(queued.front());
        queued.erase(queued.begin());
    }
    REQUIRE_FALSE(apply_ran); // 执行前未回写
    REQUIRE(pointCount(model_layer, comp) == 3);
    commit_fn();
    {
        std::unique_lock lock(mutex);
        REQUIRE(waitForGui(lock, cv, 5s, [&] { return finished; }));
    }

    REQUIRE(raw->last_job->state() == systems::job::JobState::Done);
    REQUIRE(apply_ran);
    REQUIRE(pointCount(model_layer, comp) == 4); // 回写生效
    REQUIRE(undo.canUndo()); // undo 边界成记录
    REQUIRE(undo.undoLabel().value_or("") == "回写任务"); // label = 任务名

    undo.undo(); // 记录可撤销 → 回写走正规 undo 语义
    REQUIRE(pointCount(model_layer, comp) == 3);
    REQUIRE_FALSE(undo.canUndo());
}

TEST_CASE("Failed free job does not write back", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer);
    UndoStack undo(model_layer);
    model_layer.setUndoRecorder(&undo);
    FeatureSystem system(model_layer, bus, &undo);

    std::mutex mutex;
    std::condition_variable cv;
    std::vector<std::function<void()>> queued;
    bool finished = false;
    systems::job::JobRunner runner(model_layer, &undo, [&](std::function<void()> fn) {
        std::lock_guard lock(mutex);
        queued.push_back(std::move(fn));
        cv.notify_all();
    });
    system.setJobRunner(&runner);
    systems::job::JobState terminal_state { };
    std::string terminal_error;
    runner.setOnFinished([&](systems::job::Job& job) {
        const auto st = job.state();
        const auto err = job.error();
        std::lock_guard lock(mutex);
        finished = true;
        terminal_state = st;
        terminal_error = err;
        cv.notify_all();
    });

    auto* raw = new WritebackJobFeature;
    ModelLayer* model = &model_layer;
    raw->target = comp;
    raw->write = [model, comp](ComponentOperator&) {
        auto op = model->getComponentOperator(comp);
        if (op)
            op->appendPoint({ 9.0, 9.0, 9.0 });
    };
    raw->task = [model, comp](systems::job::ProgressFn) {
        throw std::runtime_error("compute failed");
    };
    REQUIRE(system.registerHandler(makeWritebackMeta(), FeatureSystem::SystemHandlerPtr { raw }));

    system.invoke("WritebackJobFeature");
    REQUIRE(raw->last_job != nullptr);

    // 失败结果只走所属线程收尾、不调用应用段：排空队列
    {
        std::unique_lock lock(mutex);
        REQUIRE(waitForGui(lock, cv, 5s, [&] { return !queued.empty(); }));
        auto fn = std::move(queued.front());
        queued.erase(queued.begin());
        lock.unlock();
        fn();
    }
    {
        std::unique_lock lock(mutex);
        REQUIRE(waitForGui(lock, cv, 5s, [&] { return finished; }));
    }

    REQUIRE(raw->last_job->state() == systems::job::JobState::Failed);
    REQUIRE(terminal_state == systems::job::JobState::Failed); // 终态信息送达监听（状态栏呈现依据）
    REQUIRE(terminal_error == "compute failed");
    REQUIRE(pointCount(model_layer, comp) == 3); // 零回写
    REQUIRE_FALSE(undo.canUndo()); // 零记录
}

// —— runTypedWriteback 绑定回写（写面由框架发）契约 ——

namespace {
//! 注入式任务体 + 回写段的绑定回写功能
class BoundWritebackFeature : public FeatureHandler {
public:
    std::any execute(FeatureContext& ctx) override
    {
        last_job = ctx.runTypedWriteback("绑定回写任务", target, [task = std::move(task)](systems::job::ProgressFn report) { task(report); return 0; }, [write = std::move(write)](ComponentOperator& op, int&) { write(op); });
        return { };
    }
    Index target { -1 };
    systems::job::JobTaskFn task;
    WritebackFn write;
    std::shared_ptr<systems::job::Job> last_job;
};

HandlerMetaData makeBoundWritebackMeta()
{
    HandlerMetaData meta_data;
    meta_data.name = "BoundWritebackFeature";
    meta_data.display_name = "绑定回写功能";
    meta_data.description = "runTypedWriteback 契约测试";
    return meta_data;
}
}

TEST_CASE("Writeback job receives resolved write surface inside undo boundary", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer);
    UndoStack undo(model_layer);
    model_layer.setUndoRecorder(&undo);
    FeatureSystem system(model_layer, bus, &undo);

    std::mutex mutex;
    std::condition_variable cv;
    std::vector<std::function<void()>> queued;
    bool finished = false;
    systems::job::JobRunner runner(model_layer, &undo, [&](std::function<void()> fn) {
        std::lock_guard lock(mutex);
        queued.push_back(std::move(fn));
        cv.notify_all();
    });
    system.setJobRunner(&runner);
    runner.setOnFinished([&](systems::job::Job& job) {
        std::lock_guard lock(mutex);
        finished = true;
        cv.notify_all();
    });

    auto* raw = new BoundWritebackFeature;
    raw->target = comp;
    raw->task = [](systems::job::ProgressFn) { };
    bool write_ran = false;
    raw->write = [&write_ran](ComponentOperator& op) {
        write_ran = true;
        // 写面就绪契约：目标存在与网格在场由框架保证——插件直接写，不再防御式查找
        const auto n = op.component().mesh->vertex_positions_.size();
        op.appendPoint({ static_cast<double>(n), 7.0, 7.0 });
    };
    REQUIRE(system.registerHandler(makeBoundWritebackMeta(), FeatureSystem::SystemHandlerPtr { raw }));

    system.invoke("BoundWritebackFeature");
    REQUIRE(raw->last_job != nullptr);

    // 等提交闭包入队 → 主线程执行（= 生产中的 GUI 回写段）
    std::function<void()> commit_fn;
    {
        std::unique_lock lock(mutex);
        REQUIRE(waitForGui(lock, cv, 5s, [&] { return !queued.empty(); }));
        commit_fn = std::move(queued.front());
        queued.erase(queued.begin());
    }
    REQUIRE_FALSE(write_ran); // 执行前未回写
    REQUIRE(pointCount(model_layer, comp) == 3);
    commit_fn();
    {
        std::unique_lock lock(mutex);
        REQUIRE(waitForGui(lock, cv, 5s, [&] { return finished; }));
    }

    REQUIRE(raw->last_job->state() == systems::job::JobState::Done);
    REQUIRE(write_ran);
    REQUIRE(pointCount(model_layer, comp) == 4); // 框架写面回写生效
    REQUIRE(undo.canUndo()); // undo 边界成记录（提交段在边界内执行）
    REQUIRE(undo.undoLabel().value_or("") == "绑定回写任务"); // label = 任务名

    undo.undo(); // 记录可撤销 → 回写走正规 undo 语义
    REQUIRE(pointCount(model_layer, comp) == 3);
}

TEST_CASE("In-flight structural delete rejected by write window; writeback lands", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer);
    UndoStack undo(model_layer);
    model_layer.setUndoRecorder(&undo);
    FeatureSystem system(model_layer, bus, &undo);

    std::mutex mutex;
    std::condition_variable cv;
    std::vector<std::function<void()>> queued;
    bool finished = false;
    systems::job::JobRunner runner(model_layer, &undo, [&](std::function<void()> fn) {
        std::lock_guard lock(mutex);
        queued.push_back(std::move(fn));
        cv.notify_all();
    });
    system.setJobRunner(&runner);
    runner.setOnFinished([&](systems::job::Job& job) {
        std::lock_guard lock(mutex);
        finished = true;
        cv.notify_all();
    });

    auto* raw = new BoundWritebackFeature;
    raw->target = comp;
    raw->task = [](systems::job::ProgressFn) { };
    bool write_ran = false;
    raw->write = [&write_ran](ComponentOperator&) { write_ran = true; };
    REQUIRE(system.registerHandler(makeBoundWritebackMeta(), FeatureSystem::SystemHandlerPtr { raw }));

    system.invoke("BoundWritebackFeature");
    REQUIRE(raw->last_job != nullptr);

    // 在飞期删除目标组件：结构性写同受窗保护——拒绝、组件仍在（区分一：碰模型数据）
    {
        std::string caught;
        try {
            model_layer.removeComponent(comp);
        } catch (const std::exception& e) {
            caught = e.what();
        }
        REQUIRE(caught.find("writeback is pending") != std::string::npos);
        REQUIRE(model_layer.findComponent(comp) != nullptr);
    }

    {
        std::unique_lock lock(mutex);
        REQUIRE(waitForGui(lock, cv, 5s, [&] { return !queued.empty(); }));
        auto fn = std::move(queued.front());
        queued.erase(queued.begin());
        lock.unlock();
        fn();
    }
    {
        std::unique_lock lock(mutex);
        REQUIRE(waitForGui(lock, cv, 5s, [&] { return finished; }));
    }

    REQUIRE(raw->last_job->state() == systems::job::JobState::Done);
    REQUIRE(write_ran); // 删除被拒、目标在场：回写照常落地

    // 终态关窗：结构性写恢复（写在边界内——有记账归属）
    undo.beginOperation("窗关后删除");
    REQUIRE_NOTHROW(model_layer.removeComponent(comp));
    undo.commitOperation();
    REQUIRE(model_layer.findComponent(comp) == nullptr);
}

TEST_CASE("Writeback job does not write on failed task", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer);
    UndoStack undo(model_layer);
    model_layer.setUndoRecorder(&undo);
    FeatureSystem system(model_layer, bus, &undo);

    std::mutex mutex;
    std::condition_variable cv;
    std::vector<std::function<void()>> queued;
    bool finished = false;
    systems::job::JobRunner runner(model_layer, &undo, [&](std::function<void()> fn) {
        std::lock_guard lock(mutex);
        queued.push_back(std::move(fn));
        cv.notify_all();
    });
    system.setJobRunner(&runner);
    systems::job::JobState terminal_state { };
    runner.setOnFinished([&](systems::job::Job& job) {
        const auto st = job.state();
        std::lock_guard lock(mutex);
        finished = true;
        terminal_state = st;
        cv.notify_all();
    });

    auto* raw = new BoundWritebackFeature;
    raw->target = comp;
    raw->task = [](systems::job::ProgressFn) {
        throw std::runtime_error("compute failed");
    };
    bool write_ran = false;
    raw->write = [&write_ran](ComponentOperator&) { write_ran = true; };
    REQUIRE(system.registerHandler(makeBoundWritebackMeta(), FeatureSystem::SystemHandlerPtr { raw }));

    system.invoke("BoundWritebackFeature");
    REQUIRE(raw->last_job != nullptr);

    // 失败结果只走所属线程收尾、不调用应用段：排空队列
    {
        std::unique_lock lock(mutex);
        REQUIRE(waitForGui(lock, cv, 5s, [&] { return !queued.empty(); }));
        auto fn = std::move(queued.front());
        queued.erase(queued.begin());
        lock.unlock();
        fn();
    }
    {
        std::unique_lock lock(mutex);
        REQUIRE(waitForGui(lock, cv, 5s, [&] { return finished; }));
    }

    REQUIRE(raw->last_job->state() == systems::job::JobState::Failed);
    REQUIRE(terminal_state == systems::job::JobState::Failed);
    REQUIRE_FALSE(write_ran); // 失败不回写
    REQUIRE(pointCount(model_layer, comp) == 3); // 零回写
    REQUIRE_FALSE(undo.canUndo()); // 零记录
}

TEST_CASE("Undo refuses during model operation without cancelling it", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer);
    UndoStack undo(model_layer);
    model_layer.setUndoRecorder(&undo);
    FeatureSystem system(model_layer, bus, &undo);
    systems::job::JobRunner runner(model_layer, &undo, dispatchGui); // 模型任务提交段就地执行（特权过写闸）
    system.setJobRunner(&runner);
    // 与 QModelManager 装配一致的联动钩子

    // 种子记录（4 点）
    {
        auto op = model_layer.getComponentOperator(comp);
        REQUIRE(op.has_value());
        undo.beginOperation("seed");
        op->appendPoint({ 5.0, 5.0, 5.0 });
        undo.commitOperation();
    }
    REQUIRE(undo.canUndo());

    std::mutex mutex;
    std::condition_variable cv;
    bool finished = false;
    systems::job::JobState terminal_state { };
    runner.setOnFinished([&](systems::job::Job& job) {
        const auto st = job.state();
        std::lock_guard lock(mutex);
        finished = true;
        terminal_state = st;
        cv.notify_all();
    });

    auto* raw = new ModelJobFeature;
    raw->target_component = comp;
    raw->task = [](ComponentOperator&, systems::job::ProgressFn report) -> void {
        for (;;) { // 心跳循环：取消于下次 report 抛 JobCancelledException
            report(0.5, "working");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return; // 不可达
    };
    REQUIRE(system.registerHandler(makeModelJobMetaData(), FeatureSystem::SystemHandlerPtr { raw }));

    system.invoke("ModelJobFeature");
    REQUIRE(raw->last_job != nullptr);
    REQUIRE(model_layer.writesFrozen());

    REQUIRE_NOTHROW(undo.undo()); // 冻结期撤销：优雅拒绝，不撞写闸抛异常
    REQUIRE(undo.canUndo()); // 栈未被改动
    REQUIRE_FALSE(raw->last_job->isCancellationRequested());
    raw->last_job->cancel();
    {
        std::unique_lock lock(mutex);
        REQUIRE(waitForGui(lock, cv, 5s, [&] { return finished; })); // 钩子取消已生效
    }
    REQUIRE(raw->last_job->state() == systems::job::JobState::Cancelled);
    REQUIRE(terminal_state == systems::job::JobState::Cancelled); // 取消终态送达监听
    REQUIRE_FALSE(model_layer.writesFrozen()); // 窗口随任务终态关闭

    undo.undo(); // 重试成功
    REQUIRE_FALSE(undo.canUndo());
    REQUIRE(pointCount(model_layer, comp) == 3); // 种子记录已回退
}

TEST_CASE("Undo refuses during free job and succeeds after explicit cancellation", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer);
    UndoStack undo(model_layer);
    model_layer.setUndoRecorder(&undo);
    FeatureSystem system(model_layer, bus, &undo);
    systems::job::JobRunner runner(model_layer, &undo, dispatchGui);
    system.setJobRunner(&runner);

    // 种子记录（4 点）
    {
        auto op = model_layer.getComponentOperator(comp);
        REQUIRE(op.has_value());
        undo.beginOperation("seed");
        op->appendPoint({ 6.0, 6.0, 6.0 });
        undo.commitOperation();
    }
    REQUIRE(undo.canUndo());

    std::mutex mutex;
    std::condition_variable cv;
    bool finished = false;
    runner.setOnFinished([&](systems::job::Job& job) {
        std::lock_guard lock(mutex);
        finished = true;
        cv.notify_all();
    });

    auto* raw = new WritebackJobFeature;
    raw->task = [](systems::job::ProgressFn report) -> void {
        for (;;) { // 自由任务心跳循环（无 apply：提交闭包零开销空操作）
            report(0.4, "working");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return; // 不可达
    };
    REQUIRE(system.registerHandler(makeWritebackMeta(), FeatureSystem::SystemHandlerPtr { raw }));

    system.invoke("WritebackJobFeature");
    REQUIRE(raw->last_job != nullptr);
    REQUIRE_FALSE(model_layer.writesFrozen()); // 自由任务不开冻结窗（软冻结窗另计）

    REQUIRE_NOTHROW(undo.undo());
    REQUIRE_FALSE(raw->last_job->isCancellationRequested());
    REQUIRE(pointCount(model_layer, comp) == 4);
    raw->last_job->cancel(); // 非冻结：撤销正常执行
    REQUIRE(undo.canUndo());
    REQUIRE(pointCount(model_layer, comp) == 4);
    {
        std::unique_lock lock(mutex);
        REQUIRE(waitForGui(lock, cv, 5s, [&] { return finished; })); // 撤销联动取消已生效
    }
    REQUIRE(raw->last_job->state() == systems::job::JobState::Cancelled);
    undo.undo();
    REQUIRE(pointCount(model_layer, comp) == 3);
}

// —— 功能退出与忙时注册表隔离 ——

TEST_CASE("Busy feature registration and removal have no lifecycle or cancellation effects", "[FeatureSystem][operation]")
{
    class TrackedFeature : public WritebackJobFeature {
    public:
        int& deactivated;
        int& torn_down;
        TrackedFeature(int& deactivated, int& torn_down)
            : deactivated(deactivated)
            , torn_down(torn_down)
        {
        }
        void deactivate(FeatureContext&) override { ++deactivated; }
        void teardown(FeatureContext&) override { ++torn_down; }
    };
    core::EventBus bus;
    ModelLayer model;
    const auto target = addTriangleComponent(model);
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    int deactivated = 0, torn_down = 0, infos_changed = 0;
    FeatureSystem system(model, bus, &undo);
    OwnerQueue queue;
    systems::job::JobRunner runner(model, &undo, queue.dispatcher());
    system.setJobRunner(&runner);
    system.setOnFeatureInfosChanged([&] { ++infos_changed; });
    auto handler = std::make_unique<TrackedFeature>(deactivated, torn_down);
    auto* raw = handler.get();
    raw->target = target;
    raw->task = [](systems::job::ProgressFn) { };
    bool applied = false;
    raw->write = [&](ComponentOperator& op) { applied = true; op.appendPoint({ 1, 0, 0 }); };
    REQUIRE(system.registerHandler(makeWritebackMeta(), FeatureSystem::SystemHandlerPtr { handler.release() }));
    REQUIRE(system.setFeatureActive("WritebackJobFeature"));
    system.invoke("WritebackJobFeature");
    const auto job = raw->last_job;
    REQUIRE(job);
    auto complete = queue.take();
    const auto* params = system.params("WritebackJobFeature");
    REQUIRE_THROWS_AS(system.unregisterHandler(makeWritebackMeta()), ModelOperationBusy);
    REQUIRE_THROWS_AS(system.registerHandler(makeWritebackMeta(), FeatureSystem::SystemHandlerPtr { std::make_unique<FakeFeatureHandler>().release() }), ModelOperationBusy);
    REQUIRE_THROWS_AS(system.registerHandler(makeMetaData(), FeatureSystem::SystemHandlerPtr { std::make_unique<FakeFeatureHandler>().release() }), ModelOperationBusy);
    REQUIRE_FALSE(job->isCancellationRequested());
    REQUIRE(system.params("WritebackJobFeature") == params);
    REQUIRE_FALSE(system.params("FakeFeature"));
    REQUIRE(infos_changed == 1);
    REQUIRE(deactivated == 0);
    REQUIRE(torn_down == 0);
    complete();
    REQUIRE(job->state() == systems::job::JobState::Done);
    REQUIRE(applied);
    REQUIRE(pointCount(model, target) == 4);
    REQUIRE_NOTHROW(system.unregisterHandler(makeWritebackMeta()));
    REQUIRE(deactivated == 1);
    REQUIRE(torn_down == 1);
    REQUIRE(infos_changed == 2);
    REQUIRE_FALSE(system.params("WritebackJobFeature"));
    // 析构期间信息回调捕获的计数器必须仍存活。
    system.setOnFeatureInfosChanged([] { });
}

TEST_CASE("Switching out of a feature cancels its in-flight job", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer); // 回写目标
    UndoStack undo(model_layer);
    model_layer.setUndoRecorder(&undo);
    FeatureSystem system(model_layer, bus, &undo);

    std::mutex mutex;
    std::condition_variable cv;
    bool finished = false;
    systems::job::JobRunner runner(model_layer, &undo, dispatchGui); // 就地分发（取消场景不触提交）
    system.setJobRunner(&runner);
    runner.setOnFinished([&](systems::job::Job& job) {
        std::lock_guard lock(mutex);
        finished = true;
        cv.notify_all();
    });

    auto* raw = new WritebackJobFeature;
    bool apply_ran = false;
    raw->target = comp;
    raw->write = [&apply_ran](ComponentOperator&) { apply_ran = true; };
    raw->task = [&apply_ran](systems::job::ProgressFn report) {
        for (int i = 0; i < 1000; ++i) {
            report(0.4, "working");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };
    REQUIRE(system.registerHandler(makeWritebackMeta(), FeatureSystem::SystemHandlerPtr { raw }));

    REQUIRE(system.setFeatureActive("WritebackJobFeature")); // 进入功能
    system.invoke("WritebackJobFeature");
    REQUIRE(raw->last_job != nullptr);
    auto job = raw->last_job;

    REQUIRE(system.setFeatureActive("")); // 切出 → 框架取消本功能在飞任务

    {
        std::unique_lock lock(mutex);
        REQUIRE(waitForGui(lock, cv, 5s, [&] { return finished; }));
    }
    REQUIRE(job->state() == systems::job::JobState::Cancelled);
    REQUIRE_FALSE(apply_ran); // 退出流程开始后回写不再落地
}

TEST_CASE("Rejected removal of another feature leaves the running job intact", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer);
    UndoStack undo(model_layer);
    model_layer.setUndoRecorder(&undo);
    FeatureSystem system(model_layer, bus, &undo);

    std::mutex mutex;
    std::condition_variable cv;
    std::vector<std::function<void()>> queued;
    bool finished = false;
    systems::job::JobRunner runner(model_layer, &undo, [&](std::function<void()> fn) {
        std::lock_guard lock(mutex);
        queued.push_back(std::move(fn));
        cv.notify_all();
    });
    system.setJobRunner(&runner);
    runner.setOnFinished([&](systems::job::Job& job) {
        std::lock_guard lock(mutex);
        finished = true;
        cv.notify_all();
    });

    // 功能 A：有限长任务（300ms 后正常完成回写）
    auto* raw = new WritebackJobFeature;
    bool apply_ran = false;
    ModelLayer* model = &model_layer;
    raw->target = comp;
    raw->write = [model, comp, &apply_ran](ComponentOperator&) {
        apply_ran = true;
        auto op = model->getComponentOperator(comp);
        if (op)
            op->appendPoint({ 3.0, 3.0, 3.0 });
    };
    raw->task = [&apply_ran, model, comp](systems::job::ProgressFn report) {
        for (int i = 0; i <= 30; ++i) {
            report(i / 30.0, "working");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };
    REQUIRE(system.registerHandler(makeWritebackMeta(), FeatureSystem::SystemHandlerPtr { raw }));
    // 功能 B：无任务（作注销隔离的对照）
    REQUIRE(system.registerHandler(makeMetaData(), FeatureSystem::SystemHandlerPtr { new FakeFeatureHandler }));

    system.invoke("WritebackJobFeature");
    REQUIRE(raw->last_job != nullptr);
    auto job = raw->last_job;

    REQUIRE_THROWS_AS(system.unregisterHandler(makeMetaData()), ModelOperationBusy); // 忙时注销 B 拒绝，不取消 A
    REQUIRE(system.params("FakeFeature"));
    REQUIRE_FALSE(job->isCancellationRequested());

    {
        std::unique_lock lock(mutex);
        REQUIRE(waitForGui(lock, cv, 5s, [&] { return !queued.empty(); })); // A 正常走到提交
    }
    queued.front()();
    queued.erase(queued.begin());
    {
        std::unique_lock lock(mutex);
        REQUIRE(waitForGui(lock, cv, 5s, [&] {
            if (!queued.empty()) {
                auto fn = std::move(queued.front());
                queued.erase(queued.begin());
                lock.unlock();
                fn();
                lock.lock();
            }
            return finished;
        }));
    }
    REQUIRE(job->state() == systems::job::JobState::Done); // A 未被误取消
    REQUIRE(apply_ran); // 回写正常落地
    REQUIRE(pointCount(model_layer, comp) == 4);
}

// —— P1.5 类型化回写糖：compute 返回值经框架槽直传 write ——

namespace {
//! 类型化回写功能：payload 经框架结果槽直传 write（免共享状态手搭）
class TypedWritebackFeature : public FeatureHandler {
public:
    std::any execute(FeatureContext& ctx) override
    {
        last_job = ctx.runTypedWriteback("类型化回写任务", target, [payload = this->payload, fail = this->fail_compute](systems::job::ProgressFn) {
                if (fail)
                    throw std::runtime_error("compute failed"); // 抛在 compute 内（worker）→ 任务 Failed
                return payload; }, [this](ComponentOperator& op, int& value) {
                write_ran = true;
                received = value;
                op.appendPoint({ static_cast<double>(value), 0.0, 0.0 }); });
        return { };
    }
    Index target { -1 };
    int payload { 5 };
    bool fail_compute { false }; //!< compute 抛异常 → 任务 Failed、write 不触达
    bool write_ran { false };
    int received { -1 };
    std::shared_ptr<systems::job::Job> last_job;
};

HandlerMetaData makeTypedWritebackMeta()
{
    HandlerMetaData meta_data;
    meta_data.name = "TypedWritebackFeature";
    meta_data.display_name = "类型化回写功能";
    meta_data.description = "runTypedWriteback 契约测试";
    return meta_data;
}
}

TEST_CASE("Typed writeback passes compute result straight to write", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer);
    UndoStack undo(model_layer);
    model_layer.setUndoRecorder(&undo);
    FeatureSystem system(model_layer, bus, &undo);

    std::mutex mutex;
    std::condition_variable cv;
    std::vector<std::function<void()>> queued;
    bool finished = false;
    systems::job::JobRunner runner(model_layer, &undo, [&](std::function<void()> fn) {
        std::lock_guard lock(mutex);
        queued.push_back(std::move(fn));
        cv.notify_all();
    });
    system.setJobRunner(&runner);
    runner.setOnFinished([&](systems::job::Job& job) {
        std::lock_guard lock(mutex);
        finished = true;
        cv.notify_all();
    });

    auto* raw = new TypedWritebackFeature;
    raw->target = comp;
    raw->payload = 7;
    REQUIRE(system.registerHandler(makeTypedWritebackMeta(), FeatureSystem::SystemHandlerPtr { raw }));

    system.invoke("TypedWritebackFeature");
    REQUIRE(raw->last_job != nullptr);

    std::function<void()> commit_fn;
    {
        std::unique_lock lock(mutex);
        REQUIRE(waitForGui(lock, cv, 5s, [&] { return !queued.empty(); }));
        commit_fn = std::move(queued.front());
        queued.erase(queued.begin());
    }
    REQUIRE_FALSE(raw->write_ran); // 执行前未回写
    commit_fn();
    {
        std::unique_lock lock(mutex);
        REQUIRE(waitForGui(lock, cv, 5s, [&] { return finished; }));
    }

    REQUIRE(raw->last_job->state() == systems::job::JobState::Done);
    REQUIRE(raw->write_ran);
    REQUIRE(raw->received == 7); // compute 返回值经框架槽直传 write
    REQUIRE(pointCount(model_layer, comp) == 4); // 回写生效（坐标 = payload）
    REQUIRE(undo.canUndo());
    REQUIRE(undo.undoLabel().value_or("") == "类型化回写任务");
}

TEST_CASE("Typed writeback skips write when compute throws", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer);
    UndoStack undo(model_layer);
    model_layer.setUndoRecorder(&undo);
    FeatureSystem system(model_layer, bus, &undo);

    std::mutex mutex;
    std::condition_variable cv;
    bool finished = false;
    systems::job::JobRunner runner(model_layer, &undo, dispatchGui); // 就地分发（不触提交）
    system.setJobRunner(&runner);
    systems::job::JobState terminal_state { };
    std::string terminal_error;
    runner.setOnFinished([&](systems::job::Job& job) {
        const auto st = job.state();
        const auto err = job.error();
        std::lock_guard lock(mutex);
        finished = true;
        terminal_state = st;
        terminal_error = err;
        cv.notify_all();
    });

    auto* raw = new TypedWritebackFeature;
    raw->target = comp;
    raw->fail_compute = true;
    REQUIRE(system.registerHandler(makeTypedWritebackMeta(), FeatureSystem::SystemHandlerPtr { raw }));

    system.invoke("TypedWritebackFeature");
    REQUIRE(raw->last_job != nullptr);

    {
        std::unique_lock lock(mutex);
        REQUIRE(waitForGui(lock, cv, 5s, [&] { return finished; }));
    }
    REQUIRE(raw->last_job->state() == systems::job::JobState::Failed); // compute 抛异常 → Failed
    REQUIRE(terminal_state == systems::job::JobState::Failed);
    REQUIRE(terminal_error == "compute failed");
    REQUIRE_FALSE(raw->write_ran); // 提交段不执行 → write 不触达（结果槽契约）
    REQUIRE(pointCount(model_layer, comp) == 3); // 零回写
    REQUIRE_FALSE(undo.canUndo()); // 零记录
}

// —— 功能退出层会话兜底关闭（框架接管，插件零样板） ——

namespace {
//! 开层并预览写一点（模拟预览插件的层开启，handler 自身不关层）
void openScopePreview(ModelLayer& model_layer, UndoStack& undo, Index comp, const std::array<double, 3>& before)
{
    REQUIRE(undo.beginScope("预览"));
    auto op = model_layer.getComponentOperator(comp);
    REQUIRE(op.has_value());
    op->editableMesh(MeshEditKind::NonTopology).vertex_positions_[0] = { 9.0, 9.0, 9.0 };
    REQUIRE(model_layer.findComponent(comp)->mesh->vertex_positions_[0] != before);
    REQUIRE(undo.scopeActive());
}

//! 退出后断言：会话已关 + 坐标恢复 before₀
void requireScopeClosedRestored(ModelLayer& model_layer, UndoStack& undo, Index comp,
    const std::array<double, 3>& before)
{
    REQUIRE_FALSE(undo.scopeActive());
    REQUIRE(model_layer.findComponent(comp)->mesh->vertex_positions_[0] == before);
}
}

TEST_CASE("Switching out of a feature closes any open scope session", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer);
    const auto before = model_layer.findComponent(comp)->mesh->vertex_positions_[0];
    UndoStack undo(model_layer);
    model_layer.setUndoRecorder(&undo);
    FeatureSystem system(model_layer, bus, &undo);
    REQUIRE(system.registerHandler(makeMetaData(), FeatureSystem::SystemHandlerPtr { new FakeFeatureHandler }));

    openScopePreview(model_layer, undo, comp, before); // 边界外预览写：层保持（深度 0 归层不隐式杀）

    REQUIRE(system.setFeatureActive("FakeFeature")); // 进入（Fake 零会话管理——插件样板已删）
    REQUIRE(system.setFeatureActive("")); // 切出 → 框架在 deactivate 回调后兜底关会话

    requireScopeClosedRestored(model_layer, undo, comp, before);
}

TEST_CASE("Unregistering a feature closes any open scope session", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer);
    const auto before = model_layer.findComponent(comp)->mesh->vertex_positions_[0];
    UndoStack undo(model_layer);
    model_layer.setUndoRecorder(&undo);
    FeatureSystem system(model_layer, bus, &undo);
    REQUIRE(system.registerHandler(makeMetaData(), FeatureSystem::SystemHandlerPtr { new FakeFeatureHandler }));

    openScopePreview(model_layer, undo, comp, before);

    system.unregisterHandler(makeMetaData()); // 非 active 注销：teardown 回调后兜底关会话

    requireScopeClosedRestored(model_layer, undo, comp, before);
}

TEST_CASE("Feature system teardown closes any open scope session", "[FeatureSystem]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer);
    const auto before = model_layer.findComponent(comp)->mesh->vertex_positions_[0];
    UndoStack undo(model_layer);
    model_layer.setUndoRecorder(&undo);

    {
        FeatureSystem system(model_layer, bus, &undo);
        REQUIRE(system.registerHandler(makeMetaData(), FeatureSystem::SystemHandlerPtr { new FakeFeatureHandler }));
        openScopePreview(model_layer, undo, comp, before);
    } // 系统析构：teardown 循环后、flush 前兜底关会话（栈与模型层存活）

    requireScopeClosedRestored(model_layer, undo, comp, before);
}

TEST_CASE("FeatureSystem commit gate rejects submission inside undo boundary (G3 end-to-end)",
    "[FeatureSystem][G3]")
{
    // 端到端：Runner 构造绑定 undo 栈，提交门查询 inOperation。边界内 submit 的
    // 完成通知到达时正式边界仍打开 → 立即拒绝（影子不落、模型零变化）；边界收尾后同一
    // runner 正常放行。提交闭包排队、测试线程手动泵（模拟生产事件循环"下一轮"到达）：
    // 泵时机在 invoke 返回后 → 门查询与边界开关同线程同序，无跨线程时序竞态。
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer);
    UndoStack undo(model_layer);
    model_layer.setUndoRecorder(&undo);
    FeatureSystem system(model_layer, bus, &undo);

    std::mutex mutex;
    std::condition_variable cv;
    std::vector<std::function<void()>> queued;
    int finished_count = 0;
    std::string last_error;
    systems::job::JobRunner runner(model_layer, &undo, [&](std::function<void()> fn) {
        std::lock_guard lock(mutex);
        queued.push_back(std::move(fn));
        cv.notify_all();
    });
    system.setJobRunner(&runner);
    runner.setOnFinished([&](systems::job::Job& job) {
        const auto error = job.error();
        std::lock_guard lock(mutex);
        ++finished_count;
        last_error = error;
        cv.notify_all();
    });

    // 所属线程消费完成通知，应用或拒绝后终态计数 +1。
    auto pumpUntil = [&](int target) {
        for (;;) {
            std::function<void()> fn;
            {
                std::unique_lock lock(mutex);
                if (finished_count >= target)
                    return;
                if (!waitForGui(lock, cv, 5s,
                        [&] { return finished_count >= target || !queued.empty(); }))
                    return; // 超时：由调用方断言终态数给出失败
                if (finished_count >= target)
                    return;
                fn = std::move(queued.front());
                queued.erase(queued.begin());
            }
            fn();
        }
    };

    auto* raw = new ModelJobFeature;
    raw->target_component = comp;
    raw->task = [](ComponentOperator& shadow_target, systems::job::ProgressFn) {
        shadow_target.appendPoint({ 9.0, 9.0, 9.0 });
    };
    REQUIRE(system.registerHandler(makeModelJobMetaData(), FeatureSystem::SystemHandlerPtr { raw }));

    // 边界未结束却驱动完成通知：拒绝重入应用，任务只收尾一次。
    {
        ModelScope outer(model_layer, &undo, "外部边界（插队窗口）");
        system.invoke("ModelJobFeature");
        REQUIRE(raw->last_job != nullptr);
        pumpUntil(1);
        REQUIRE(finished_count == 1);
        REQUIRE(raw->last_job->state() == systems::job::JobState::Failed);
        REQUIRE(last_error.find("boundary") != std::string::npos);
        REQUIRE(pointCount(model_layer, comp) == 3); // 影子未落：提交体被拒
    } // 空边界收尾（无记录）
    REQUIRE_FALSE(undo.canUndo());

    // 边界收尾后同一 runner：门开 → 正常提交（泵在 invoke 返回后 = 事件循环下一轮语义）
    system.invoke("ModelJobFeature");
    pumpUntil(2);
    REQUIRE(finished_count == 2);
    REQUIRE(raw->last_job->state() == systems::job::JobState::Done);
    REQUIRE(pointCount(model_layer, comp) == 4);
    REQUIRE(undo.canUndo());
}

// —— 软冻结（pending）：登记 / 双挂释放 / owner 豁免 / 锁泄漏矩阵 ——

namespace {
//! queued 分发装配（提交闭包排队，测试线程手动执行——模拟生产 GUI 事件循环时机）
struct PendingHarness {
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<std::function<void()>> queued;
    bool finished { false };
    systems::job::JobState terminal_state { };
    std::string terminal_error;

    std::function<void(std::function<void()>)> dispatcher()
    {
        return [this](std::function<void()> fn) {
            std::lock_guard lock(mutex);
            queued.push_back(std::move(fn));
            cv.notify_all();
        };
    }
    void bindFinished(systems::job::JobRunner& runner)
    {
        runner.setOnFinished([this](systems::job::Job& job) {
            const auto st = job.state();
            const auto err = job.error();
            std::lock_guard lock(mutex);
            finished = true;
            terminal_state = st;
            terminal_error = err;
            cv.notify_all();
        });
    }
    //! 取出排队闭包（不执行——断言 pending 窗口后再手动 pump）
    std::function<void()> takeOne()
    {
        std::unique_lock lock(mutex);
        REQUIRE(waitForGui(lock, cv, 5s, [this] { return !queued.empty(); }));
        std::function<void()> fn = std::move(queued.front());
        queued.erase(queued.begin());
        return fn;
    }
    void waitFinished()
    {
        std::unique_lock lock(mutex);
        REQUIRE(waitForGui(lock, cv, 5s, [this, &lock] {
            if (!queued.empty()) {
                auto fn = std::move(queued.front());
                queued.erase(queued.begin());
                lock.unlock();
                fn();
                lock.lock();
            }
            return finished;
        }));
    }
};

//! GUI 写面闭包（appendPoint），不携活模型到 worker。
WritebackFn makePointWrite()
{
    return [](ComponentOperator& op) { op.appendPoint({ 9.0, 9.0, 9.0 }); };
}
}

TEST_CASE("R3 pending window rejects GUI write, owner writeback lands, Done releases", "[FeatureSystem][R3]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer);
    UndoStack undo(model_layer);
    model_layer.setUndoRecorder(&undo);
    FeatureSystem system(model_layer, bus, &undo);
    PendingHarness h;
    systems::job::JobRunner runner(model_layer, &undo, h.dispatcher());
    system.setJobRunner(&runner);
    h.bindFinished(runner);

    auto* raw = new WritebackJobFeature;
    raw->target = comp;
    raw->task = [](systems::job::ProgressFn) { };
    raw->write = makePointWrite();
    REQUIRE(system.registerHandler(makeWritebackMeta(), FeatureSystem::SystemHandlerPtr { raw }));

    system.invoke("WritebackJobFeature");
    REQUIRE(raw->last_job != nullptr);
    auto commit_fn = h.takeOne(); // 提交闭包排队（开窗在提交即已发生、此刻仍开着）
    REQUIRE(model_layer.writesPending());
    REQUIRE(model_layer.writesPending()); // 状态栏显形数据源转发

    // ① pending 期 GUI 写拒绝 + 零修改（写前拦截）
    {
        auto op = model_layer.getComponentOperator(comp);
        REQUIRE(op.has_value()); // 断言移出 try：Catch 断言异常不得被本用例 catch 吞掉
        std::string caught;
        try {
            op->appendPoint({ 4.0, 4.0, 4.0 });
        } catch (const std::exception& e) {
            caught = e.what();
        }
        REQUIRE(caught.find("writeback is pending") != std::string::npos);
        REQUIRE(pointCount(model_layer, comp) == 3);
    }

    // ② owner 兑现：提交段 Privilege 放行 → 回写生效
    commit_fn();
    h.waitFinished();
    REQUIRE(raw->last_job->state() == systems::job::JobState::Done);
    REQUIRE(pointCount(model_layer, comp) == 4);
    REQUIRE(undo.canUndo()); // 回写在 undo 边界内成记录

    // ③ 终态释放 + 写恢复（恢复 = 写闸不再拒 pending；写入在边界内）
    REQUIRE_FALSE(model_layer.writesPending());
    {
        auto op = model_layer.getComponentOperator(comp);
        REQUIRE(op.has_value());
        undo.beginOperation("恢复写");
        REQUIRE_NOTHROW(op->appendPoint({ 5.0, 5.0, 5.0 }));
        undo.commitOperation();
    }
    REQUIRE(pointCount(model_layer, comp) == 5);
}

TEST_CASE("R3 cancel retains operation until queued commit is discarded", "[FeatureSystem][R3]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer);
    FeatureSystem system(model_layer, bus);
    PendingHarness h;
    systems::job::JobRunner runner(model_layer, nullptr, h.dispatcher());
    system.setJobRunner(&runner);
    h.bindFinished(runner);

    auto* raw = new WritebackJobFeature;
    raw->target = comp;
    raw->task = [](systems::job::ProgressFn) { };
    raw->write = makePointWrite();
    REQUIRE(system.registerHandler(makeWritebackMeta(), FeatureSystem::SystemHandlerPtr { raw }));

    system.invoke("WritebackJobFeature");
    REQUIRE(raw->last_job != nullptr);
    auto commit_fn = h.takeOne();
    REQUIRE(model_layer.writesPending());

    raw->last_job->cancel();
    REQUIRE(model_layer.writesPending()); // 请求取消尚未完成提交丢弃

    commit_fn(); // 排队期取消拦截：提交体不执行
    h.waitFinished();
    REQUIRE(h.terminal_state == systems::job::JobState::Cancelled);
    REQUIRE(pointCount(model_layer, comp) == 3); // 回写未发生
    REQUIRE_FALSE(model_layer.writesPending());
}

TEST_CASE("R3 runner stop cancels queued completion and releases pending on owner thread", "[FeatureSystem][R3]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer);
    FeatureSystem system(model_layer, bus);
    PendingHarness h;
    systems::job::JobRunner runner(model_layer, nullptr, h.dispatcher());
    system.setJobRunner(&runner);
    h.bindFinished(runner);

    auto* raw = new WritebackJobFeature;
    raw->target = comp;
    raw->task = [](systems::job::ProgressFn) { };
    raw->write = makePointWrite();
    REQUIRE(system.registerHandler(makeWritebackMeta(), FeatureSystem::SystemHandlerPtr { raw }));

    system.invoke("WritebackJobFeature");
    REQUIRE(raw->last_job != nullptr);
    (void)h.takeOne(); // 闭包留在队列不执行
    REQUIRE(model_layer.writesPending());

    runner.stop(); // join 计算后在所属线程丢弃结果、清理和释放
    h.waitFinished(); // stop 同步完成 GUI 收尾与终态通知
    REQUIRE_FALSE(model_layer.writesPending()); // stop() 经 join 保证释放已完成
    REQUIRE(h.terminal_state == systems::job::JobState::Cancelled); // stop 请求取消，排队结果不应用
    REQUIRE(pointCount(model_layer, comp) == 3); // 提交体未执行
}

TEST_CASE("R3 pending released when writeback apply throws (Failed)", "[FeatureSystem][R3]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer);
    FeatureSystem system(model_layer, bus);
    PendingHarness h;
    systems::job::JobRunner runner(model_layer, nullptr, h.dispatcher());
    system.setJobRunner(&runner);
    h.bindFinished(runner);

    auto* raw = new WritebackJobFeature;
    raw->target = comp;
    raw->write = [](ComponentOperator&) { throw std::runtime_error("apply boom"); };
    raw->task = [](systems::job::ProgressFn) {
    };
    REQUIRE(system.registerHandler(makeWritebackMeta(), FeatureSystem::SystemHandlerPtr { raw }));

    system.invoke("WritebackJobFeature");
    REQUIRE(raw->last_job != nullptr);
    auto commit_fn = h.takeOne();
    REQUIRE(model_layer.writesPending());

    commit_fn(); // apply 抛 → 提交段 catch 回写 error → Failed → release
    h.waitFinished();
    REQUIRE(h.terminal_state == systems::job::JobState::Failed);
    REQUIRE(h.terminal_error.find("apply boom") != std::string::npos);
    REQUIRE_FALSE(model_layer.writesPending());
}

TEST_CASE("R3 free job locks model data for whole in-flight window", "[FeatureSystem][R3]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer);
    FeatureSystem system(model_layer, bus);
    PendingHarness h;
    systems::job::JobRunner runner(model_layer, nullptr, h.dispatcher());
    system.setJobRunner(&runner);
    h.bindFinished(runner);

    auto* raw = new WritebackJobFeature;
    raw->task = [](systems::job::ProgressFn) { }; // 无 apply
    REQUIRE(system.registerHandler(makeWritebackMeta(), FeatureSystem::SystemHandlerPtr { raw }));

    system.invoke("WritebackJobFeature");
    REQUIRE(raw->last_job != nullptr);
    auto commit_fn = h.takeOne();
    REQUIRE(model_layer.writesPending()); // 提交即开窗（不再看 apply）：在飞期锁模型数据
    REQUIRE(pointCount(model_layer, comp) == 3); // 不碰模型数据的只读查询照常过闸

    commit_fn();
    h.waitFinished();
    REQUIRE(h.terminal_state == systems::job::JobState::Done);
    REQUIRE_FALSE(model_layer.writesPending());
}

TEST_CASE("R3 undo during pending writeback refuses without cancelling the task", "[FeatureSystem][R3]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index comp = addTriangleComponent(model_layer);
    UndoStack undo(model_layer);
    model_layer.setUndoRecorder(&undo);
    FeatureSystem system(model_layer, bus, &undo);
    PendingHarness h;
    systems::job::JobRunner runner(model_layer, &undo, h.dispatcher());
    system.setJobRunner(&runner);
    h.bindFinished(runner);

    // 种子记录（4 点）
    {
        auto op = model_layer.getComponentOperator(comp);
        REQUIRE(op.has_value());
        undo.beginOperation("seed");
        op->appendPoint({ 6.0, 6.0, 6.0 });
        undo.commitOperation();
    }
    REQUIRE(undo.canUndo());

    auto* raw = new WritebackJobFeature;
    raw->target = comp;
    raw->task = [](systems::job::ProgressFn) { };
    raw->write = makePointWrite();
    REQUIRE(system.registerHandler(makeWritebackMeta(), FeatureSystem::SystemHandlerPtr { raw }));

    system.invoke("WritebackJobFeature");
    REQUIRE(raw->last_job != nullptr);
    auto commit_fn = h.takeOne(); // pending 开窗（登记后、提交前）
    REQUIRE(model_layer.writesPending());

    REQUIRE_NOTHROW(undo.undo());
    REQUIRE(model_layer.writesPending());
    REQUIRE(undo.canUndo());
    REQUIRE(pointCount(model_layer, comp) == 4);
    REQUIRE_FALSE(raw->last_job->isCancellationRequested());
    raw->last_job->cancel();
    REQUIRE(model_layer.writesPending());

    commit_fn(); // 晚到闭包执行（Qt 生产中事件循环必达）：排队期取消拦截 → 完成等待
    h.waitFinished(); // 钩子取消生效：任务 Cancelled
    REQUIRE(h.terminal_state == systems::job::JobState::Cancelled);
    REQUIRE(pointCount(model_layer, comp) == 4); // 种子记录未提前撤销
    undo.undo();
    REQUIRE(pointCount(model_layer, comp) == 3);
    REQUIRE_FALSE(model_layer.writesPending()); // 终态幂等释放
}

TEST_CASE("Rejected repeated model task never releases the running model operation", "[FeatureSystem][operation]")
{
    core::EventBus bus;
    ModelLayer layer;
    const Index comp = addTriangleComponent(layer);
    FeatureSystem system(layer, bus);
    PendingHarness h;
    systems::job::JobRunner runner(layer, nullptr, h.dispatcher());
    system.setJobRunner(&runner);
    h.bindFinished(runner);
    auto* raw = new ModelJobFeature;
    raw->target_component = comp;
    raw->task = [](ComponentOperator&, systems::job::ProgressFn) { };
    REQUIRE(system.registerHandler(makeModelJobMetaData(), FeatureSystem::SystemHandlerPtr { raw }));
    system.invoke("ModelJobFeature");
    auto commit = h.takeOne();
    auto rejected = raw->context->runModelJob("second", comp, raw->task);
    REQUIRE_FALSE(rejected);
    REQUIRE(layer.writesFrozen());
    REQUIRE_THROWS_AS(layer.getComponentOperator(comp)->appendPoint({ 8, 8, 8 }), ModelOperationBusy);
    commit();
    h.waitFinished();
    REQUIRE_FALSE(layer.writesPending());
    REQUIRE(pointCount(layer, comp) == 3);
}

TEST_CASE("Synchronous algorithm cannot bypass a free task's model operation", "[FeatureSystem][operation]")
{
    class WritingAlgorithm final : public systems::algo::AlgorithmHandler {
    public:
        std::any execute(systems::algo::HandlerContext& ctx, const std::vector<core::ArgObject>&) override
        {
            ctx.cur_component.appendPoint({ 8, 8, 8 });
            return { };
        }
        std::vector<core::ArgType> args_type() const override { return { }; }
    };
    core::EventBus bus;
    ModelLayer layer;
    const Index comp = addTriangleComponent(layer);
    FeatureSystem system(layer, bus);
    systems::io::ModelIOSystem io(layer);
    systems::algo::AlgorithmSystem algorithms(io, layer);
    systems::algo::HandlerMetaData meta;
    meta.name = "Writer";
    REQUIRE(algorithms.registerHandler(meta, systems::algo::AlgorithmSystem::SystemHandlerPtr { new WritingAlgorithm }));
    PendingHarness h;
    systems::job::JobRunner runner(layer, nullptr, h.dispatcher());
    system.setJobRunner(&runner);
    h.bindFinished(runner);
    auto* raw = new WritebackJobFeature;
    raw->task = [](systems::job::ProgressFn) { };
    REQUIRE(system.registerHandler(makeWritebackMeta(), FeatureSystem::SystemHandlerPtr { raw }));
    system.invoke("WritebackJobFeature");
    auto commit = h.takeOne();
    algorithms.call("Writer", comp, { });
    REQUIRE(pointCount(layer, comp) == 3);
    REQUIRE(layer.writesPending());
    commit();
    h.waitFinished();
    algorithms.call("Writer", comp, { });
    REQUIRE(pointCount(layer, comp) == 4);
}
TEST_CASE("Captured writeback owns all three phases and releases preparation failures", "[FeatureSystem][operation]")
{
    class CapturedFeature final : public FeatureHandler {
    public:
        FeatureContext* context { };
        Index target { -1 };
        std::thread::id gui_thread { std::this_thread::get_id() };
        bool fail_capture { false };
        bool captured_on_gui { false };
        bool computed_on_worker { false };
        bool wrote_on_gui { false };
        std::shared_ptr<systems::job::Job> job;
        void setup(FeatureRegistrar&, FeatureContext& ctx) override { context = &ctx; }
        std::any execute(FeatureContext& ctx) override
        {
            job = ctx.runTypedWriteback("captured", target, [this](const ComponentOperator& op) {
                    captured_on_gui = std::this_thread::get_id() == gui_thread;
                    if (fail_capture)
                        throw std::runtime_error("capture failed");
                    return op.mesh()->vertex_positions_.size(); }, [this](std::size_t& count, systems::job::ProgressFn) {
                    computed_on_worker = std::this_thread::get_id() != gui_thread;
                    return count; }, [this](ComponentOperator& op, std::size_t& count) {
                    wrote_on_gui = std::this_thread::get_id() == gui_thread;
                    op.appendPoint({ static_cast<double>(count), 0, 0 }); });
            return { };
        }
    };
    core::EventBus bus;
    ModelLayer model;
    const Index comp = addTriangleComponent(model);
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    FeatureSystem system(model, bus, &undo);
    PendingHarness h;
    systems::job::JobRunner runner(model, &undo, h.dispatcher());
    system.setJobRunner(&runner);
    h.bindFinished(runner);
    auto handler = std::make_unique<CapturedFeature>();
    auto* raw = handler.get();
    HandlerMetaData metadata;
    metadata.name = "Captured";
    REQUIRE(system.registerHandler(metadata, FeatureSystem::SystemHandlerPtr { handler.release() }));
    raw->target = comp;
    // 没有正式入口或本功能预览时，在复制输入之前拒绝。
    raw->execute(*raw->context);
    REQUIRE_FALSE(raw->job);
    REQUIRE_FALSE(raw->captured_on_gui);
    auto event_sub = raw->context->events.subscribe<GateTestEvent>([&](const GateTestEvent&) {
        raw->execute(*raw->context);
    });
    bus.publish(GateTestEvent { });
    REQUIRE_FALSE(raw->job);
    REQUIRE_FALSE(raw->captured_on_gui);
    raw->fail_capture = true;
    REQUIRE_THROWS_AS(system.invoke("Captured"), std::runtime_error);
    REQUIRE_FALSE(model.writesPending());
    REQUIRE_FALSE(runner.currentJob());
    REQUIRE_FALSE(undo.canUndo());
    raw->fail_capture = false;
    system.invoke("Captured");
    auto commit = h.takeOne();
    REQUIRE(model.writesPending());
    commit();
    h.waitFinished();
    REQUIRE(raw->job->state() == systems::job::JobState::Done);
    REQUIRE(raw->captured_on_gui);
    REQUIRE(raw->computed_on_worker);
    REQUIRE(raw->wrote_on_gui);
    REQUIRE(pointCount(model, comp) == 4);
    REQUIRE(undo.undo());
    REQUIRE(pointCount(model, comp) == 3);

    undo.beginOperation("write then submit");
    model.getComponentOperator(comp)->appendPoint({ 5, 5, 5 });
    REQUIRE_FALSE(raw->context->runJob("invalid", [](systems::job::ProgressFn) { }));
    REQUIRE_FALSE(model.writesPending());
    undo.commitOperation();
    REQUIRE(undo.undo());
    REQUIRE(pointCount(model, comp) == 3);
}
TEST_CASE("Cancellation without a heartbeat keeps model authority until calculation exits", "[FeatureSystem][operation]")
{
    core::EventBus bus;
    ModelLayer model;
    const Index comp = addTriangleComponent(model);
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    undo.beginOperation("seed");
    model.getComponentOperator(comp)->appendPoint({ 4, 0, 0 });
    undo.commitOperation();
    FeatureSystem system(model, bus, &undo);
    PendingHarness h;
    systems::job::JobRunner runner(model, &undo, h.dispatcher());
    system.setJobRunner(&runner);
    h.bindFinished(runner);
    std::mutex mutex;
    std::condition_variable cv;
    bool started = false;
    bool released = false;
    auto handler = std::make_unique<WritebackJobFeature>();
    auto* raw = handler.get();
    handler->task = [&](systems::job::ProgressFn) {
        std::unique_lock lock(mutex);
        started = true;
        cv.notify_all();
        cv.wait(lock, [&] { return released; }); // 无心跳，取消不能替代真正返回。
    };
    REQUIRE(system.registerHandler(makeWritebackMeta(), FeatureSystem::SystemHandlerPtr { handler.release() }));
    system.invoke("WritebackJobFeature");
    {
        std::unique_lock lock(mutex);
        CHECK(cv.wait_for(lock, 5s, [&] { return started; }));
    }
    raw->last_job->cancel();
    CHECK(model.writesPending());
    CHECK(raw->last_job->state() == systems::job::JobState::Running);
    CHECK_FALSE(undo.undo());
    CHECK(pointCount(model, comp) == 4);
    CHECK_THROWS_AS(model.getComponentOperator(comp)->appendPoint({ 5, 0, 0 }), ModelOperationBusy);
    {
        std::lock_guard lock(mutex);
        released = true;
    }
    cv.notify_all();
    h.waitFinished();
    REQUIRE(raw->last_job->state() == systems::job::JobState::Cancelled);
    REQUIRE_FALSE(model.writesPending());
    REQUIRE(undo.undo());
    REQUIRE(pointCount(model, comp) == 3);
}

TEST_CASE("Preview job writes only the captured scope and execute confirms it", "[FeatureSystem][preview][operation]")
{
    class PreviewJobFeature : public FeatureHandler {
    public:
        Index target;
        FeatureContext* context { nullptr };
        std::shared_ptr<systems::job::Job> job;
        core::EventBus::Subscription sub;
        void setup(FeatureRegistrar&, FeatureContext& ctx) override
        {
            context = &ctx;
            sub = ctx.events.subscribe<GateTestEvent>([this](const GateTestEvent&) {
                REQUIRE(context->undo.beginScope("preview job"));
                job = context->runTypedWriteback("preview compute", target, [](systems::job::ProgressFn) { return 7; }, [](ComponentOperator& op, int& value) { op.appendPoint({ double(value), 0, 0 }); });
            });
        }
    };
    core::EventBus bus;
    ModelLayer model;
    const Index target = addTriangleComponent(model);
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    FeatureSystem system(model, bus, &undo);
    PendingHarness h;
    systems::job::JobRunner runner(model, &undo, h.dispatcher());
    system.setJobRunner(&runner);
    h.bindFinished(runner);
    auto* raw = new PreviewJobFeature;
    raw->target = target;
    HandlerMetaData meta;
    meta.name = "PreviewJob";
    meta.display_name = "PreviewJob";
    REQUIRE(system.registerHandler(meta, FeatureSystem::SystemHandlerPtr { raw }));
    bus.publish(GateTestEvent { });
    REQUIRE(raw->job);
    auto commit = h.takeOne();
    REQUIRE(model.writesPending());
    REQUIRE_FALSE(undo.undo());
    SECTION("success")
    {
        commit();
        h.waitFinished();
        REQUIRE(raw->job->state() == systems::job::JobState::Done);
        REQUIRE(raw->context->undo.scopeActive());
        REQUIRE(pointCount(model, target) == 4);
        system.invoke("PreviewJob");
        REQUIRE_FALSE(undo.scopeActive());
        REQUIRE(undo.undo());
        REQUIRE(pointCount(model, target) == 3);
        REQUIRE_FALSE(undo.canUndo());
        REQUIRE(undo.redo());
        REQUIRE(pointCount(model, target) == 4);
    }
    SECTION("cancel retains occupation until real finish")
    {
        raw->context->undo.cancelScope();
        REQUIRE(model.writesPending());
        commit();
        h.waitFinished();
        REQUIRE(raw->job->state() == systems::job::JobState::Cancelled);
        REQUIRE_FALSE(undo.scopeActive());
        REQUIRE_FALSE(undo.canUndo());
        REQUIRE(pointCount(model, target) == 3);
    }
    SECTION("pending preview cannot be replaced through plugin interfaces")
    {
        REQUIRE_THROWS_AS(raw->context->undo.beginScope("replacement"), ModelOperationBusy);
        commit();
        h.waitFinished();
        REQUIRE(raw->job->state() == systems::job::JobState::Done);
        REQUIRE(pointCount(model, target) == 4);
        raw->context->undo.cancelScope();
        REQUIRE(pointCount(model, target) == 3);
        REQUIRE_FALSE(undo.canUndo());
    }
}

TEST_CASE("Execute refuses task publication after a nonempty preview and closes it", "[FeatureSystem][preview][operation]")
{
    class PublishingFeature : public FeatureHandler {
    public:
        std::shared_ptr<systems::job::Job> job;
        std::any execute(FeatureContext& ctx) override
        {
            job = ctx.runJob("must refuse", [](systems::job::ProgressFn) { });
            return { };
        }
    };
    core::EventBus bus;
    ModelLayer model;
    const Index target = addTriangleComponent(model);
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    FeatureSystem system(model, bus, &undo);
    systems::job::JobRunner runner(model, &undo, dispatchGui);
    system.setJobRunner(&runner);
    auto* raw = new PublishingFeature;
    HandlerMetaData meta;
    meta.name = "Publishing";
    REQUIRE(system.registerHandler(meta, FeatureSystem::SystemHandlerPtr { raw }));
    {
        UndoStack::OwnerScope owner(&undo, "Publishing");
        REQUIRE(undo.beginScope("existing preview"));
        model.getComponentOperator(target)->appendPoint({ 4, 4, 4 });
    }
    system.invoke("Publishing");
    REQUIRE_FALSE(raw->job);
    REQUIRE_FALSE(model.writesPending());
    REQUIRE_FALSE(undo.scopeActive());
    REQUIRE(undo.undo());
    REQUIRE(pointCount(model, target) == 3);
    REQUIRE_FALSE(undo.canUndo());
}

TEST_CASE("Feature destruction waits for a task already cancelled by feature exit", "[FeatureSystem][operation]")
{
    ModelLayer model;
    const Index component = addTriangleComponent(model);
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    core::EventBus bus;
    PendingHarness harness;
    systems::job::JobRunner runner(model, &undo, harness.dispatcher());
    auto system = std::make_unique<FeatureSystem>(model, bus, &undo);
    system->setJobRunner(&runner);
    harness.bindFinished(runner);
    auto handler = std::make_unique<WritebackJobFeature>();
    auto* raw = handler.get();
    raw->target = component;
    raw->task = [](systems::job::ProgressFn) { };
    raw->write = makePointWrite();
    REQUIRE(system->registerHandler(makeWritebackMeta(), FeatureSystem::SystemHandlerPtr { handler.release() }));
    REQUIRE(system->setFeatureActive("WritebackJobFeature"));
    system->invoke("WritebackJobFeature");
    auto queued_commit = harness.takeOne();
    auto job = raw->last_job;
    REQUIRE(job);
    REQUIRE(system->setFeatureActive(""));
    REQUIRE(job->isCancellationRequested());
    REQUIRE(model.writesPending());
    system.reset();
    REQUIRE(job->state() == systems::job::JobState::Cancelled);
    REQUIRE_FALSE(model.writesPending());
    REQUIRE_FALSE(runner.currentJob());
    queued_commit(); // 系统与 handler 已销毁，迟到闭包必须空转。
    REQUIRE(pointCount(model, component) == 3);
    REQUIRE_FALSE(undo.canUndo());
}

TEST_CASE("Pure preview calculation neither confirms nor closes its existing preview", "[FeatureSystem][preview][operation]")
{
    class PurePreviewFeature : public FeatureHandler {
    public:
        Index target;
        FeatureContext* context { nullptr };
        core::EventBus::Subscription sub;
        std::shared_ptr<systems::job::Job> job;
        void setup(FeatureRegistrar&, FeatureContext& ctx) override
        {
            context = &ctx;
            sub = ctx.events.subscribe<GateTestEvent>([this](const GateTestEvent&) {
                REQUIRE(context->undo.beginScope("pure preview"));
                context->model.getComponentOperator(target)->appendPoint({ 7, 0, 0 });
                job = context->runJob("compute only", [](systems::job::ProgressFn) { });
            });
        }
    };
    core::EventBus bus;
    ModelLayer model;
    const Index target = addTriangleComponent(model);
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    FeatureSystem system(model, bus, &undo);
    PendingHarness h;
    systems::job::JobRunner runner(model, &undo, h.dispatcher());
    system.setJobRunner(&runner);
    h.bindFinished(runner);
    auto* raw = new PurePreviewFeature;
    raw->target = target;
    HandlerMetaData meta;
    meta.name = "PurePreview";
    meta.display_name = "PurePreview";
    REQUIRE(system.registerHandler(meta, FeatureSystem::SystemHandlerPtr { raw }));
    bus.publish(GateTestEvent { });
    REQUIRE(raw->job);
    h.takeOne()();
    h.waitFinished();
    REQUIRE(raw->job->state() == systems::job::JobState::Done);
    REQUIRE(undo.scopeActive());
    REQUIRE(pointCount(model, target) == 4);
    SECTION("execute creates exactly one formal record")
    {
        system.invoke("PurePreview");
        REQUIRE_FALSE(undo.scopeActive());
        REQUIRE(undo.undo());
        REQUIRE(pointCount(model, target) == 3);
        REQUIRE_FALSE(undo.canUndo());
    }
    SECTION("cancelling preview leaves no formal record")
    {
        raw->context->undo.cancelScope();
        REQUIRE_FALSE(undo.scopeActive());
        REQUIRE(pointCount(model, target) == 3);
        REQUIRE_FALSE(undo.canUndo());
    }
}

TEST_CASE("Busy feature switching applies only the last valid target", "[FeatureSystem][operation]")
{
    ModelLayer model;
    core::EventBus bus;
    FeatureSystem system(model, bus);
    OwnerQueue queue;
    systems::job::JobRunner runner(model, nullptr, queue.dispatcher());
    system.setJobRunner(&runner);
    auto install = [&](const std::string& name) {
        auto handler = std::make_unique<FakeFeatureHandler>();
        auto* raw = handler.get();
        HandlerMetaData meta;
        meta.name = name;
        REQUIRE(system.registerHandler(meta, FeatureSystem::SystemHandlerPtr { handler.release() }));
        return raw;
    };
    auto* a = install("A");
    auto* b = install("B");
    auto* c = install("C");
    REQUIRE(system.setFeatureActive("A"));
    auto job = runner.run("hold A", [] { return systems::job::JobWork { [](systems::job::ProgressFn) { } }; }, { "A" });
    REQUIRE(job);
    auto complete = queue.take();
    int a_exits = 1, b_entries = 0, c_entries = 0;
    bool cancelled = true;
    SECTION("A to B to C skips B")
    {
        REQUIRE(system.setFeatureActive("B"));
        REQUIRE(system.setFeatureActive("C"));
        c_entries = 1;
    }
    SECTION("A to B to A preserves the active session")
    {
        REQUIRE(system.setFeatureActive("B"));
        REQUIRE(system.setFeatureActive("A"));
        a_exits = 0;
    }
    SECTION("invalid target cannot overwrite B")
    {
        REQUIRE(system.setFeatureActive("B"));
        REQUIRE_FALSE(system.setFeatureActive("missing"));
        b_entries = 1;
    }
    SECTION("invalid first target does not cancel the running job")
    {
        REQUIRE_FALSE(system.setFeatureActive("missing"));
        REQUIRE_FALSE(job->isCancellationRequested());
        REQUIRE(system.setFeatureActive("B"));
        b_entries = 1;
    }
    SECTION("empty final target exits without entering B")
    {
        REQUIRE(system.setFeatureActive("B"));
        REQUIRE(system.setFeatureActive(""));
    }
    SECTION("same target without pending switch is inert")
    {
        REQUIRE(system.setFeatureActive("A"));
        a_exits = 0;
        cancelled = false;
    }
    REQUIRE(model.writesPending());
    REQUIRE(a->deactivate_count == 0);
    REQUIRE(b->activate_count == 0);
    REQUIRE(c->activate_count == 0);
    complete();
    REQUIRE(job->state() == (cancelled ? systems::job::JobState::Cancelled : systems::job::JobState::Done));
    REQUIRE_FALSE(model.writesPending());
    REQUIRE(a->deactivate_count == a_exits);
    REQUIRE(b->activate_count == b_entries);
    REQUIRE(c->activate_count == c_entries);
    REQUIRE(b->deactivate_count == 0);
}

TEST_CASE("Cleanup callback switching is deferred and does not cancel a foreign job", "[FeatureSystem][operation]")
{
    class HookFeature : public FakeFeatureHandler {
    public:
        std::function<void()> on_exit;
        void deactivate(FeatureContext& ctx) override
        {
            FakeFeatureHandler::deactivate(ctx);
            on_exit();
        }
    };
    ModelLayer model;
    core::EventBus bus;
    FeatureSystem system(model, bus);
    OwnerQueue queue;
    systems::job::JobRunner runner(model, nullptr, queue.dispatcher());
    system.setJobRunner(&runner);
    auto a_handler = std::make_unique<HookFeature>();
    auto b_handler = std::make_unique<FakeFeatureHandler>();
    auto c_handler = std::make_unique<FakeFeatureHandler>();
    auto* a = a_handler.get();
    auto* b = b_handler.get();
    auto* c = c_handler.get();
    HandlerMetaData meta;
    meta.name = "A";
    REQUIRE(system.registerHandler(meta, FeatureSystem::SystemHandlerPtr { a_handler.release() }));
    meta.name = "B";
    REQUIRE(system.registerHandler(meta, FeatureSystem::SystemHandlerPtr { b_handler.release() }));
    meta.name = "C";
    REQUIRE(system.registerHandler(meta, FeatureSystem::SystemHandlerPtr { c_handler.release() }));
    a->on_exit = [&] {
        CHECK(model.hasWritePrivilege());
        CHECK(system.setFeatureActive("C"));
        CHECK(b->activate_count == 0);
        CHECK(c->activate_count == 0);
    };
    REQUIRE(system.setFeatureActive("A"));
    auto job = runner.run("foreign", [] { return systems::job::JobWork { [](systems::job::ProgressFn) { } }; }, { "Another" });
    auto complete = queue.take();
    REQUIRE(system.setFeatureActive("B"));
    REQUIRE_FALSE(job->isCancellationRequested());
    complete();
    REQUIRE(job->state() == systems::job::JobState::Done);
    REQUIRE(a->deactivate_count == 1);
    REQUIRE(b->activate_count == 1);
    REQUIRE(b->deactivate_count == 1);
    REQUIRE(c->activate_count == 1);
    REQUIRE_FALSE(model.writesPending());
}

TEST_CASE("Feature switching rejects occupancy without a cleanup dispatcher", "[FeatureSystem][operation]")
{
    ModelLayer model;
    core::EventBus bus;
    FeatureSystem system(model, bus);
    auto handler = std::make_unique<FakeFeatureHandler>();
    auto* raw = handler.get();
    REQUIRE(system.registerHandler(makeMetaData(), FeatureSystem::SystemHandlerPtr { handler.release() }));
    auto operation = model.beginWriteOperation();
    REQUIRE_FALSE(system.setFeatureActive("FakeFeature"));
    REQUIRE(raw->activate_count == 0);
    operation.reset();
    REQUIRE(system.setFeatureActive("FakeFeature"));
    REQUIRE(raw->activate_count == 1);
}

TEST_CASE("Entering a feature during an ownerless job preserves its computation", "[FeatureSystem][operation]")
{
    ModelLayer model;
    core::EventBus bus;
    FeatureSystem system(model, bus);
    OwnerQueue queue;
    systems::job::JobRunner runner(model, nullptr, queue.dispatcher());
    system.setJobRunner(&runner);
    auto handler = std::make_unique<FakeFeatureHandler>();
    auto* raw = handler.get();
    REQUIRE(system.registerHandler(makeMetaData(), FeatureSystem::SystemHandlerPtr { handler.release() }));
    auto job = runner.run("algorithm-like", [] { return systems::job::JobWork { [](systems::job::ProgressFn) { } }; });
    REQUIRE(job);
    auto complete = queue.take();
    REQUIRE(system.setFeatureActive("FakeFeature"));
    REQUIRE_FALSE(job->isCancellationRequested());
    REQUIRE(raw->activate_count == 0);
    complete();
    REQUIRE(job->state() == systems::job::JobState::Done);
    REQUIRE(raw->activate_count == 1);
}

TEST_CASE("Typed move-only payload is destroyed on owner before deferred cleanup", "[FeatureSystem][operation][lifetime]")
{
    struct Lifetime {
        std::vector<std::thread::id> destroyed_on;
        int writes { 0 };
    } lifetime;
    struct Probe {
        Lifetime& lifetime;
        int count;
        Probe(Lifetime& lifetime, int count)
            : lifetime(lifetime)
            , count(count)
        {
        }
        ~Probe() { lifetime.destroyed_on.push_back(std::this_thread::get_id()); }
    };
    // 输入／结果不可默认构造也不可复制；中间移动不销毁实际载荷。
    struct Payload {
        std::unique_ptr<Probe> probe;
        Payload(Lifetime& lifetime, int count)
            : probe(std::make_unique<Probe>(lifetime, count))
        {
        }
        Payload(Payload&&) noexcept = default;
        Payload(const Payload&) = delete;
    };
    class PayloadFeature final : public FeatureHandler {
    public:
        Lifetime& lifetime_;
        Index target_;
        bool fail_compute_ { false };
        std::shared_ptr<systems::job::Job> job_;
        PayloadFeature(Lifetime& lifetime, Index target)
            : lifetime_(lifetime)
            , target_(target)
        {
        }
        std::any execute(FeatureContext& ctx) override
        {
            job_ = ctx.runTypedWriteback("move payload", target_, [this](const ComponentOperator& op) { return Payload(lifetime_, static_cast<int>(op.mesh()->vertex_positions_.size())); }, [this](Payload& input, systems::job::ProgressFn) {
                    if (fail_compute_)
                        throw std::runtime_error("payload compute failed");
                    return Payload(lifetime_, input.probe->count); }, [this](ComponentOperator& op, Payload& result) {
                    ++lifetime_.writes;
                    op.appendPoint({ double(result.probe->count), 0, 0 }); });
            return { };
        }
    };

    OwnerQueue queue;
    ModelLayer model;
    const Index target = addTriangleComponent(model);
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    systems::job::JobRunner runner(model, &undo, queue.dispatcher());
    core::EventBus bus;
    FeatureSystem system(model, bus, &undo);
    system.setJobRunner(&runner);
    auto handler = std::make_unique<PayloadFeature>(lifetime, target);
    auto* raw = handler.get();
    REQUIRE(system.registerHandler({ "PayloadFeature", "载荷测试" },
        FeatureSystem::SystemHandlerPtr { handler.release() }));

    auto expected = systems::job::JobState::Done;
    SECTION("successful writeback") { }
    SECTION("failed computation")
    {
        raw->fail_compute_ = true;
        expected = systems::job::JobState::Failed;
    }
    SECTION("queued cancellation") { expected = systems::job::JobState::Cancelled; }
    system.invoke("PayloadFeature");
    auto completion = queue.take();
    REQUIRE(lifetime.destroyed_on.empty());
    if (expected == systems::job::JobState::Cancelled)
        raw->job_->cancel();

    const auto owner_thread = std::this_thread::get_id();
    const std::size_t payload_count = raw->fail_compute_ ? 1 : 2;
    bool cleaned = false;
    REQUIRE(runner.deferUntilFinished([&] {
        CHECK(model.writesPending());
        CHECK(lifetime.destroyed_on.size() == payload_count);
        for (auto destroyed_on : lifetime.destroyed_on)
            CHECK(destroyed_on == owner_thread);
        cleaned = true;
    }));
    completion();
    REQUIRE(cleaned);
    REQUIRE_FALSE(model.writesPending());
    REQUIRE(raw->job_->state() == expected);
    REQUIRE(lifetime.writes == (expected == systems::job::JobState::Done ? 1 : 0));
    REQUIRE(pointCount(model, target) == (expected == systems::job::JobState::Done ? 4 : 3));
    REQUIRE(undo.canUndo() == (expected == systems::job::JobState::Done));
}

TEST_CASE("Feature parameter gateway isolates owners while explicit global subscriptions receive all", "[FeatureSystem][events]")
{
    class ParameterFeature final : public FeatureHandler {
    public:
        FeatureContext* context_ { nullptr };
        std::vector<std::size_t> received_;
        core::EventBus::Subscription subscription_;
        void setup(FeatureRegistrar& reg, FeatureContext& ctx) override
        {
            context_ = &ctx;
            reg.addParameter({ ArgTypeEnum::Float, "数值", "1.0" });
            reg.addParameter({ ArgTypeEnum::Button, "按钮", "" });
            // 插件不识别注册名，也不手写归属过滤。
            subscription_ = ctx.events.subscribe<ParameterChangedEvent>([this](const ParameterChangedEvent& event) {
                received_.push_back(event.param_index);
            });
        }
    };
    ModelLayer model;
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    core::EventBus bus;
    FeatureSystem system(model, bus, &undo);
    auto a = std::make_unique<ParameterFeature>();
    auto b = std::make_unique<ParameterFeature>();
    auto* raw_a = a.get();
    auto* raw_b = b.get();
    REQUIRE(system.registerHandler({ "A", "功能 A" }, FeatureSystem::SystemHandlerPtr { a.release() }));
    REQUIRE(system.registerHandler({ "B", "功能 B" }, FeatureSystem::SystemHandlerPtr { b.release() }));
    std::vector<std::string> global_received;
    auto global = bus.subscribe<ParameterChangedEvent>([&](const ParameterChangedEvent& event) {
        global_received.push_back(event.feature);
    });
    std::vector<std::string> explicit_received;
    auto explicit_global = raw_a->context_->events.bus().subscribe<ParameterChangedEvent>([&](const ParameterChangedEvent& event) {
        explicit_received.push_back(event.feature);
    });

    REQUIRE(system.setParameter("A", 0, core::ArgObject::create<ArgTypeEnum::Float>(2.0)));
    REQUIRE(raw_a->received_ == std::vector<std::size_t> { 0 });
    REQUIRE(raw_b->received_.empty());
    REQUIRE(system.setParameter("B", 1, core::ArgObject::create<ArgTypeEnum::Button>(1)));
    REQUIRE(raw_a->received_ == std::vector<std::size_t> { 0 });
    REQUIRE(raw_b->received_ == std::vector<std::size_t> { 1 });
    REQUIRE(global_received == std::vector<std::string> { "A", "B" });
    REQUIRE(explicit_received == global_received);
    REQUIRE_FALSE(undo.inOperation());
    REQUIRE_FALSE(undo.canUndo());
}

TEST_CASE("Feature services keep stable addresses while registrations grow", "[FeatureSystem][registration]")
{
    core::EventBus bus;
    ModelLayer model;
    int refreshed = 0;
    FeatureSystem system(model, bus);
    system.setRenderRefreshCallback([&] { ++refreshed; });
    auto handler = std::make_unique<FakeFeatureHandler>();
    auto* raw = handler.get();
    auto meta = makeMetaData();
    meta.interactive = true;
    REQUIRE(system.registerHandler(meta, FeatureSystem::SystemHandlerPtr { handler.release() }));
    auto* context = raw->context;
    auto* params = &context->params;
    auto* events = &context->events;
    auto* interaction = &context->interaction;
    auto* info = system.getFeatureInfos().front();
    REQUIRE(system.setFeatureActive(meta.name));
    auto* state = system.activeInteraction();
    REQUIRE(state);

    // 足够多的不同注册触发节点表扩容；旧功能仍持原上下文和内部引用。
    for (int index = 0; index < 256; ++index) {
        auto other = std::make_unique<FakeFeatureHandler>();
        REQUIRE(system.registerHandler({ "Other" + std::to_string(index), "其他功能" },
            FeatureSystem::SystemHandlerPtr { other.release() }));
    }
    REQUIRE(raw->context == context);
    REQUIRE(&context->params == params);
    REQUIRE(&context->events == events);
    REQUIRE(&context->interaction == interaction);
    REQUIRE(system.params(meta.name) == params);
    REQUIRE(system.activeInteraction() == state);
    bool found_info = false;
    for (auto* current : system.getFeatureInfos()) {
        if (current->name == meta.name) {
            REQUIRE(current == info);
            found_info = true;
        }
    }
    REQUIRE(found_info);
    REQUIRE(system.setParameter(meta.name, 0, core::ArgObject::create<ArgTypeEnum::Float>(2.75)));
    REQUIRE(raw->last_float_value == 2.75);
    REQUIRE(*params->value(0).get<ArgTypeEnum::Float>() == 2.75);
    state->needs_refresh = false;
    const int previous_refreshes = refreshed;
    interaction->requestRefresh();
    REQUIRE(state->needs_refresh);
    REQUIRE(refreshed == previous_refreshes + 1);
    REQUIRE(std::any_cast<int>(system.invoke(meta.name)) == 42);
}

TEST_CASE("Feature setup can grow registrations before returning or throwing", "[FeatureSystem][registration]")
{
    struct SetupResult {
        int destroyed { 0 };
        int parameter_events { 0 };
    } result;
    class RegisteringFeature final : public FakeFeatureHandler {
    public:
        RegisteringFeature(FeatureSystem& system, SetupResult& result, bool fail)
            : system_(system)
            , result_(result)
            , fail_(fail)
        {
        }
        ~RegisteringFeature() override { ++result_.destroyed; }
        void setup(FeatureRegistrar& reg, FeatureContext& ctx) override
        {
            FakeFeatureHandler::setup(reg, ctx);
            subscription_ = ctx.events.subscribe<ParameterChangedEvent>([&result = result_](const ParameterChangedEvent&) {
                ++result.parameter_events;
            });
            for (int index = 0; index < 256; ++index) {
                auto other = std::make_unique<FakeFeatureHandler>();
                REQUIRE(system_.registerHandler({ "Child" + std::to_string(index), "子功能" },
                    FeatureSystem::SystemHandlerPtr { other.release() }));
            }
            CHECK(context == &ctx);
            CHECK(ctx.params.count() == 0); // setup 后才载入参数声明，地址不随扩容改变。
            if (fail_)
                throw std::runtime_error("setup failed after registration growth");
        }

    private:
        FeatureSystem& system_;
        SetupResult& result_;
        bool fail_;
        core::EventBus::Subscription subscription_;
    };
    bool fail = false;
    SECTION("setup completes") { }
    SECTION("setup fails after growing the map") { fail = true; }
    core::EventBus bus;
    ModelLayer model;
    FeatureSystem system(model, bus);
    auto handler = std::make_unique<RegisteringFeature>(system, result, fail);
    if (fail) {
        REQUIRE_THROWS_AS(system.registerHandler(makeMetaData(),
                              FeatureSystem::SystemHandlerPtr { handler.release() }),
            std::runtime_error);
        REQUIRE(result.destroyed == 1);
        REQUIRE(system.params("FakeFeature") == nullptr);
        REQUIRE(system.getFeatureInfos().size() == 256);
        bus.publish(ParameterChangedEvent { "FakeFeature", 0, core::ArgObject::create<ArgTypeEnum::Float>(4.0) });
        REQUIRE(result.parameter_events == 0);
        auto replacement = std::make_unique<FakeFeatureHandler>();
        REQUIRE(system.registerHandler(makeMetaData(), FeatureSystem::SystemHandlerPtr { replacement.release() }));
    } else {
        REQUIRE(system.registerHandler(makeMetaData(), FeatureSystem::SystemHandlerPtr { handler.release() }));
        REQUIRE(result.destroyed == 0);
        REQUIRE(system.getFeatureInfos().size() == 257);
        REQUIRE(system.setParameter("FakeFeature", 0, core::ArgObject::create<ArgTypeEnum::Float>(4.0)));
        REQUIRE(result.parameter_events == 1);
        REQUIRE(system.params("FakeFeature")->count() == 2);
    }
    // 当前条目失败或成功均不影响 setup 中已经完成的其他注册。
    REQUIRE(system.params("Child255")->count() == 2);
    REQUIRE(std::any_cast<int>(system.invoke("Child255")) == 42);
}

TEST_CASE("Feature registration and runner binding reject wrong threads before lifecycle changes", "[FeatureSystem][registration][thread]")
{
    class CountingSetupHandler final : public FakeFeatureHandler {
    public:
        explicit CountingSetupHandler(int& calls)
            : calls_(calls)
        {
        }
        void setup(FeatureRegistrar& registrar, FeatureContext& context) override
        {
            ++calls_;
            FakeFeatureHandler::setup(registrar, context);
        }

    private:
        int& calls_;
    };
    ModelLayer model;
    core::EventBus bus;
    OwnerQueue queue;
    systems::job::JobRunner runner(model, nullptr, queue.dispatcher());
    FeatureSystem system(model, bus);
    const auto meta = makeMetaData();
    auto original = std::make_unique<FakeFeatureHandler>();
    auto* handler = original.get();
    REQUIRE(system.registerHandler(meta, FeatureSystem::SystemHandlerPtr { original.release() }));
    system.setJobRunner(&runner);
    REQUIRE(system.setFeatureActive(meta.name));
    const auto infos = system.getFeatureInfos();
    auto* parameters = system.params(meta.name);
    int setup_calls = 0;
    int notifications = 0;
    system.setOnFeatureInfosChanged([&] { ++notifications; });
    std::array<std::exception_ptr, 5> errors;
    std::thread wrong_thread([&] {
        const std::array<std::function<void()>, 5> actions {
            [&] {
                auto replacement = std::make_unique<CountingSetupHandler>(setup_calls);
                auto fresh_meta = meta;
                fresh_meta.name = "FreshFeature";
                system.registerHandler(fresh_meta, FeatureSystem::SystemHandlerPtr { replacement.release() });
            },
            [&] {
                auto replacement = std::make_unique<CountingSetupHandler>(setup_calls);
                system.registerHandler(meta, FeatureSystem::SystemHandlerPtr { replacement.release() });
            },
            [&] { system.unregisterHandler(meta); },
            [&] { system.setJobRunner(nullptr); },
            [&] { system.setJobRunner(&runner); }
        };
        for (std::size_t i = 0; i < actions.size(); ++i) {
            try {
                actions[i]();
            } catch (...) {
                errors[i] = std::current_exception();
            }
        }
    });
    wrong_thread.join();
    for (auto error : errors) {
        REQUIRE(error);
        REQUIRE_THROWS_AS(std::rethrow_exception(error), std::runtime_error);
    }
    REQUIRE(setup_calls == 0);
    REQUIRE(notifications == 0);
    REQUIRE(handler->deactivate_count == 0);
    REQUIRE(handler->teardown_count == 0);
    REQUIRE(system.getFeatureInfos() == infos);
    REQUIRE(system.params(meta.name) == parameters);
    {
        auto occupation = model.beginWriteOperation();
        REQUIRE(occupation);
        ModelLayer::WritePrivilege privilege(model, occupation.get());
        REQUIRE_THROWS_AS(system.setJobRunner(nullptr), ModelOperationBusy);
        REQUIRE_THROWS_AS(system.setJobRunner(&runner), ModelOperationBusy);
    }
    // 被拒的注册不撤掉原参数订阅，解绑不丢失原执行器。
    REQUIRE(system.setParameter(meta.name, 0, core::ArgObject::create<ArgTypeEnum::Float>(3.25)));
    REQUIRE(handler->last_float_value == 3.25);
    auto job = handler->context->runJob("still bound", [](systems::job::ProgressFn) { });
    REQUIRE(job);
    queue.take()();
    REQUIRE(job->state() == systems::job::JobState::Done);
    REQUIRE_NOTHROW(system.setJobRunner(nullptr));
    REQUIRE_NOTHROW(system.unregisterHandler(meta));
    REQUIRE(system.getFeatureInfos().empty());
}

TEST_CASE("Interaction services are ready during setup and preserve single activation", "[FeatureSystem][interaction]")
{
    class SetupActivatingHandler final : public FakeFeatureHandler {
    public:
        void setup(FeatureRegistrar& registrar, FeatureContext& context) override
        {
            FakeFeatureHandler::setup(registrar, context);
            context.interaction.setActive(true);
        }
    };
    core::EventBus bus;
    ModelLayer model;
    FeatureSystem system(model, bus);
    int refreshes = 0;
    system.setRenderRefreshCallback([&] { ++refreshes; });
    auto first = std::make_unique<FakeFeatureHandler>();
    auto* first_handler = first.get();
    REQUIRE(system.registerHandler({ "First", "First", { }, true },
        FeatureSystem::SystemHandlerPtr { first.release() }));
    first_handler->context->interaction.setActive(true);
    auto* first_state = system.activeInteraction();
    REQUIRE(first_state);
    REQUIRE(refreshes == 1);
    auto second = std::make_unique<SetupActivatingHandler>();
    auto* second_handler = second.get();
    REQUIRE(system.registerHandler({ "Second", "Second", { }, true },
        FeatureSystem::SystemHandlerPtr { second.release() }));
    auto* second_state = system.activeInteraction();
    REQUIRE(second_state);
    REQUIRE(second_state != first_state);
    REQUIRE_FALSE(first_state->active);
    REQUIRE(second_state->active);
    REQUIRE(&second_handler->context->interaction.annotations() == &second_state->annotations);
    REQUIRE(refreshes == 2);
}

TEST_CASE("Existing interaction contexts use the current host refresh callback", "[FeatureSystem][interaction]")
{
    core::EventBus bus;
    ModelLayer model;
    FeatureSystem system(model, bus);
    auto owned = std::make_unique<FakeFeatureHandler>();
    auto* handler = owned.get();
    REQUIRE(system.registerHandler({ "Refresh", "Refresh", { }, true },
        FeatureSystem::SystemHandlerPtr { owned.release() }));
    int old_refreshes = 0;
    int new_refreshes = 0;
    system.setRenderRefreshCallback([&] { ++old_refreshes; });
    handler->context->interaction.setActive(true);
    auto* state = system.activeInteraction();
    REQUIRE(state);
    REQUIRE(old_refreshes == 1);
    state->needs_refresh = false;
    system.setRenderRefreshCallback([&] { ++new_refreshes; });
    handler->context->interaction.requestRefresh();
    REQUIRE(new_refreshes == 1);
    REQUIRE(old_refreshes == 1);
    handler->context->interaction.requestRefresh();
    REQUIRE(new_refreshes == 1);
    // 空宿主回调保持可用；重复请求仍按尚未消费的刷新合并。
    system.setRenderRefreshCallback({ });
    state->needs_refresh = false;
    REQUIRE_NOTHROW(handler->context->interaction.requestRefresh());
    REQUIRE(state->needs_refresh);
    system.setRenderRefreshCallback([&] { ++new_refreshes; });
    handler->context->interaction.requestRefresh();
    REQUIRE(new_refreshes == 1);
    state->needs_refresh = false;
    handler->context->interaction.requestRefresh();
    REQUIRE(new_refreshes == 2);
    REQUIRE(old_refreshes == 1);
}
