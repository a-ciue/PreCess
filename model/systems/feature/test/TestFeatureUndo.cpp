/**
 * @file TestFeatureUndo.cpp
 * @brief FeatureSystem undo 集成测试：边界自动记录 / 插件层会话 / 网关包装
 */
#include "ComponentData.h"
#include "ComponentOperator.h"
#include "EventBus.h"
#include "FeatureContext.h"
#include "FeatureEvents.h"
#include "FeatureHandler.h"
#include "FeatureRegistrar.h"
#include "FeatureSystem.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "ModelObserver.h"
#include "ModelScope.h"
#include "UndoStack.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include <array>
#include <memory>
#include <stdexcept>
#include <string>

struct UndoStackTestPeer {
    static std::size_t undoCount(const UndoStack& stack) { return stack.undo_.size(); }
    static std::size_t redoCount(const UndoStack& stack) { return stack.redo_.size(); }
};
using namespace systems::feature;

namespace {
struct CountingObserver : ModelObserver {
    int component_changed_count { 0 };
    bool fail_component_notification { false };

    void notifyModelChanged(Index) override { }
    void notifyModelAdded(Index) override { }
    void notifyModelRemoved(Index) override { }
    void notifyComponentRemoved(Index) override { }
    void notifyComponentChanged(Index) override
    {
        ++component_changed_count;
        if (fail_component_notification)
            throw std::runtime_error("observer failure");
    }
    void notifyModelNameChanged(Index, const std::string&) override { }
    void notifyGeometryLoadFailed(const std::string&) override { }
};

struct TestEvent { };

//! @brief 构造一个简单三角形面片组件并入池，返回 component_id
Index addTriangleComponent(ModelLayer& mgr, UndoStack& stack)
{
    std::optional<ModelScope> scope;
    if (!stack.inOperation() && !stack.scopeActive())
        scope.emplace(mgr, &stack, "添加模型");
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

    const Index model_id = mgr.addModel("undo_test", std::move(comps));
    return mgr.modelById(model_id)->componentIds()[0];
}

//! @brief 写一个点坐标（经功能上下文的 componentOperator，可写入口标脏）
void writeVertex(FeatureContext& ctx, Index component_id)
{
    auto op = ctx.componentOperator(component_id);
    if (!op)
        return;
    MeshData& md = op->editableMesh();
    md.vertex_positions_[0] = { 5.0, 5.0, 5.0 };
}

//! @brief execute 内写模型的功能（统一边界自动记录）
class WritingFeatureHandler : public FeatureHandler {
public:
    std::any execute(FeatureContext& ctx) override
    {
        ++execute_count;
        if (component_id >= 0)
            writeVertex(ctx, component_id);
        return { };
    }

    Index component_id { -1 };
    int execute_count { 0 };
};

//! @brief setup 内经 ctx.events 订阅事件、回调内写模型的功能
class EventWritingFeatureHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar&, FeatureContext& ctx) override
    {
        context = &ctx;
        sub = ctx.events.subscribe<TestEvent>([this](const TestEvent&) {
            if (component_id >= 0) {
                if (open_preview && !context->undo.scopeActive())
                    context->undo.beginScope("事件预览");
                writeVertex(*context, component_id);
            }
        });
    }

    Index component_id { -1 };
    bool open_preview { true };
    FeatureContext* context { nullptr };
    core::EventBus::Subscription sub;
};

//! @brief execute 内经 ctx.undo 的层会话写模型的功能
class ScopeFeatureHandler : public FeatureHandler {
public:
    std::any execute(FeatureContext& ctx) override
    {
        if (component_id < 0)
            return { };
        if (!ctx.undo.beginScope(label))
            return { };
        writeVertex(ctx, component_id);
        // 框架在 execute 收尾吸收并关闭预览。
        return { };
    }

    Index component_id { -1 };
    std::string label { "层操作" };
};

HandlerMetaData makeMeta(const std::string& name)
{
    HandlerMetaData meta;
    meta.name = name;
    meta.display_name = "显示_" + name;
    return meta;
}

//! @brief 公共夹具：ModelLayer + UndoStack + FeatureSystem 挂接
struct FeatureUndoFixture {
    CountingObserver obs;
    ModelLayer mgr { &obs };
    core::EventBus bus;
    UndoStack stack { mgr };
    FeatureSystem system { mgr, bus, &stack };

    FeatureUndoFixture() { mgr.setUndoRecorder(&stack); }
};
}

TEST_CASE("FeatureSystem auto feature invoke produces an undo record", "[FeatureSystem][undo]")
{
    FeatureUndoFixture f;
    const Index cid = addTriangleComponent(f.mgr, f.stack);
    f.stack.clear();

    auto* raw = new WritingFeatureHandler;
    raw->component_id = cid;
    FeatureSystem::SystemHandlerPtr handler { raw };
    REQUIRE(f.system.registerHandler(makeMeta("AutoFeature"), std::move(handler)));

    f.system.invoke("AutoFeature");
    REQUIRE(raw->execute_count == 1);
    REQUIRE(f.stack.canUndo()); // 统一边界自动记录
    REQUIRE(f.stack.undoLabel() == "显示_AutoFeature");

    // 记录可用：undo 恢复写前状态
    f.stack.undo();
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_[0]
        == std::array<double, 3> { 0.0, 0.0, 0.0 });
}

TEST_CASE("Feature invoke opens unified boundary; write evicts open scope and records", "[FeatureSystem][undo]")
{
    FeatureUndoFixture f;
    const Index cid = addTriangleComponent(f.mgr, f.stack);
    f.stack.clear();
    const int notify_before = f.obs.component_changed_count;

    // 层开在深度 0（如事件回调外的会话起点）
    REQUIRE(f.stack.beginScope("预览"));
    auto* raw = new WritingFeatureHandler;
    raw->component_id = cid;
    FeatureSystem::SystemHandlerPtr handler { raw };
    REQUIRE(f.system.registerHandler(makeMeta("UnifiedFeature"), std::move(handler)));

    // 统一边界（无模式分支）：invoke 恒开边界——边界内的写越过层 → 驱逐层
    // （回滚 before₀）→ 写归边界 → 收尾统一入栈（框架是全局栈唯一入栈者）
    f.system.invoke("UnifiedFeature");
    REQUIRE(raw->execute_count == 1);
    // 层未写过 → 驱逐为纯弹帧（first-dirty 无捕获、无回滚写）→ 仅 invoke 边界收尾 flush
    REQUIRE(f.obs.component_changed_count == notify_before + 1);
    REQUIRE_FALSE(f.stack.scopeActive()); // 层被越界写驱逐（纯弹帧关闭）
    REQUIRE(f.stack.canUndo()); // 写经统一边界成记录
    REQUIRE(f.stack.undoLabel() == "显示_UnifiedFeature");
    f.stack.undo();
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_[0]
        == std::array<double, 3> { 0.0, 0.0, 0.0 });
}

TEST_CASE("Event previews do not record until execute finalization", "[FeatureSystem][undo][preview]")
{
    FeatureUndoFixture f;
    const Index cid = addTriangleComponent(f.mgr, f.stack);
    f.stack.clear();
    auto* raw = new EventWritingFeatureHandler;
    raw->component_id = cid;
    REQUIRE(f.system.registerHandler(makeMeta("Preview"), FeatureSystem::SystemHandlerPtr { raw }));
    f.bus.publish(TestEvent { });
    f.bus.publish(TestEvent { });
    REQUIRE(f.stack.scopeActive());
    REQUIRE(UndoStackTestPeer::undoCount(f.stack) == 0);
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_[0] == std::array<double, 3> { 5, 5, 5 });
    // execute 没有新增写：已有预览仍须成为一条完整记录。
    f.system.invoke("Preview");
    REQUIRE_FALSE(f.stack.scopeActive());
    REQUIRE(UndoStackTestPeer::undoCount(f.stack) == 1);
    REQUIRE(f.stack.undo());
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_[0] == std::array<double, 3> { 0, 0, 0 });
    REQUIRE(f.stack.redo());
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_[0] == std::array<double, 3> { 5, 5, 5 });
}

TEST_CASE("Read-only events from any owner leave preview intact", "[FeatureSystem][undo][preview]")
{
    FeatureUndoFixture f;
    const Index cid = addTriangleComponent(f.mgr, f.stack);
    f.stack.clear();
    auto* raw = new EventWritingFeatureHandler;
    REQUIRE(f.system.registerHandler(makeMeta("Bystander"), FeatureSystem::SystemHandlerPtr { raw }));
    REQUIRE(f.stack.beginScope("foreign preview"));
    f.mgr.getComponentOperator(cid)->editableMesh().vertex_positions_[0] = { 5, 5, 5 };
    f.bus.publish(TestEvent { });
    REQUIRE(f.stack.scopeActive());
    REQUIRE(UndoStackTestPeer::undoCount(f.stack) == 0);
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_[0] == std::array<double, 3> { 5, 5, 5 });
    f.stack.cancelScope();
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_[0] == std::array<double, 3> { 0, 0, 0 });
}

TEST_CASE("Event cannot write without its own preview or steal a foreign preview", "[FeatureSystem][undo][preview]")
{
    FeatureUndoFixture f;
    const Index cid = addTriangleComponent(f.mgr, f.stack);
    f.stack.clear();
    auto* raw = new EventWritingFeatureHandler;
    raw->component_id = cid;
    raw->open_preview = false;
    REQUIRE(f.system.registerHandler(makeMeta("Writer"), FeatureSystem::SystemHandlerPtr { raw }));
    f.bus.publish(TestEvent { });
    REQUIRE_FALSE(f.stack.scopeActive());
    REQUIRE(UndoStackTestPeer::undoCount(f.stack) == 0);
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_[0] == std::array<double, 3> { 0, 0, 0 });
    REQUIRE(f.stack.beginScope("foreign"));
    f.mgr.getComponentOperator(cid)->editableMesh().vertex_positions_[0] = { 2, 2, 2 };
    raw->open_preview = true;
    f.bus.publish(TestEvent { });
    REQUIRE(f.stack.scopeActive());
    REQUIRE(UndoStackTestPeer::undoCount(f.stack) == 0);
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_[0] == std::array<double, 3> { 2, 2, 2 });
}

TEST_CASE("Feature records via plugin scope inside invoke boundary", "[FeatureSystem][undo]")
{
    FeatureUndoFixture f;
    const Index cid = addTriangleComponent(f.mgr, f.stack);
    f.stack.clear();

    auto* raw = new ScopeFeatureHandler;
    raw->component_id = cid;
    FeatureSystem::SystemHandlerPtr handler { raw };
    REQUIRE(f.system.registerHandler(makeMeta("ScopeFeature"), std::move(handler)));

    f.system.invoke("ScopeFeature");
    REQUIRE(f.stack.canUndo());
    REQUIRE(f.stack.undoLabel() == "层操作");
    REQUIRE_FALSE(f.stack.scopeActive()); // commitScope 后层已关闭

    f.stack.undo();
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_[0]
        == std::array<double, 3> { 0.0, 0.0, 0.0 });
}

TEST_CASE("Feature session collapses its records into one entry at deactivate", "[FeatureSystem][undo]")
{
    FeatureUndoFixture f;
    const Index cid = addTriangleComponent(f.mgr, f.stack);
    f.stack.clear();

    auto* raw = new WritingFeatureHandler;
    raw->component_id = cid;
    FeatureSystem::SystemHandlerPtr handler { raw };
    REQUIRE(f.system.registerHandler(makeMeta("SessionFeature"), std::move(handler)));

    // 进入功能 = 开会话：会话期内每步仍是独立记录、可逐步撤销
    REQUIRE(f.system.setFeatureActive("SessionFeature"));
    f.system.invoke("SessionFeature");
    f.system.invoke("SessionFeature");
    REQUIRE(f.stack.canUndo());
    REQUIRE(f.stack.undoLabel() == "显示_SessionFeature");

    // 退出功能 = 收尾折叠：两次执行压成一条 → 一步撤回会话前状态
    REQUIRE(f.system.setFeatureActive(""));
    f.stack.undo();
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_[0]
        == std::array<double, 3> { 0.0, 0.0, 0.0 });
    REQUIRE_FALSE(f.stack.canUndo()); // 未折叠的话这里还剩一条
}

TEST_CASE("Execute retains earliest preview image and closes scope on exception", "[FeatureSystem][undo][preview]")
{
    class ContinuingHandler : public FeatureHandler {
    public:
        Index cid;
        bool fail { false };
        std::any execute(FeatureContext& ctx) override
        {
            ctx.componentOperator(cid)->editableMesh().vertex_positions_[0] = { 9, 9, 9 };
            if (fail)
                throw std::runtime_error("execute failure");
            return { };
        }
    };
    FeatureUndoFixture f;
    const Index cid = addTriangleComponent(f.mgr, f.stack);
    f.stack.clear();
    auto* raw = new ContinuingHandler;
    raw->cid = cid;
    REQUIRE(f.system.registerHandler(makeMeta("Continue"), FeatureSystem::SystemHandlerPtr { raw }));
    SECTION("normal") { }
    SECTION("exception") { raw->fail = true; }
    {
        UndoStack::OwnerScope owner(&f.stack, "Continue");
        REQUIRE(f.stack.beginScope("preview"));
        f.mgr.getComponentOperator(cid)->editableMesh().vertex_positions_[0] = { 5, 5, 5 };
    }
    if (raw->fail)
        REQUIRE_THROWS_AS(f.system.invoke("Continue"), std::runtime_error);
    else
        f.system.invoke("Continue");
    REQUIRE_FALSE(f.stack.scopeActive());
    REQUIRE(UndoStackTestPeer::undoCount(f.stack) == 1);
    REQUIRE(f.stack.undo());
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_[0] == std::array<double, 3> { 0, 0, 0 });
    REQUIRE(f.stack.redo());
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_[0] == std::array<double, 3> { 9, 9, 9 });
}

TEST_CASE("Preview events preserve redo until execute commits", "[FeatureSystem][undo][preview]")
{
    FeatureUndoFixture f;
    const Index cid = addTriangleComponent(f.mgr, f.stack);
    f.stack.clear();
    f.stack.beginOperation("seed");
    f.mgr.getComponentOperator(cid)->appendPoint({ 1, 1, 1 });
    f.stack.commitOperation();
    REQUIRE(f.stack.undo());
    auto* raw = new EventWritingFeatureHandler;
    raw->component_id = cid;
    REQUIRE(f.system.registerHandler(makeMeta("Preview"), FeatureSystem::SystemHandlerPtr { raw }));
    f.bus.publish(TestEvent { });
    REQUIRE(UndoStackTestPeer::redoCount(f.stack) == 1);
    REQUIRE(UndoStackTestPeer::undoCount(f.stack) == 0);
    f.system.invoke("Preview");
    REQUIRE(UndoStackTestPeer::redoCount(f.stack) == 0);
    REQUIRE(UndoStackTestPeer::undoCount(f.stack) == 1);
}

TEST_CASE("Event callback cannot mutate history or borrow an outer boundary", "[FeatureSystem][undo][preview]")
{
    FeatureUndoFixture f;
    const Index cid = addTriangleComponent(f.mgr, f.stack);
    f.stack.clear();
    FeatureEventGateway gateway(f.bus, f.mgr, &f.stack, "callback");
    auto sub = gateway.subscribe<TestEvent>([&](const TestEvent&) {
        REQUIRE_THROWS_AS(f.stack.beginOperation("event"), ModelOperationBusy);
        REQUIRE_THROWS_AS(f.stack.clear(), ModelOperationBusy);
        REQUIRE_FALSE(f.stack.undo());
        REQUIRE_THROWS_AS(f.mgr.removeComponent(cid), ModelOperationBusy);
        REQUIRE_THROWS_AS(f.mgr.addModel("forbidden", { }), ModelOperationBusy);
        REQUIRE_THROWS_AS(f.mgr.getComponentOperator(cid)->appendPoint({ 1, 1, 1 }), ModelOperationBusy);
    });
    f.stack.beginOperation("outer");
    f.bus.publish(TestEvent { });
    f.stack.commitOperation();
    REQUIRE(UndoStackTestPeer::undoCount(f.stack) == 0);
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_.size() == 3);
}

TEST_CASE("Execute absorbs preview structural changes in order", "[FeatureSystem][undo][preview]")
{
    class RemovingHandler : public FeatureHandler {
    public:
        Index cid;
        std::any execute(FeatureContext& ctx) override
        {
            ctx.model.removeComponent(cid);
            return { };
        }
    };
    FeatureUndoFixture f;
    const Index original = addTriangleComponent(f.mgr, f.stack);
    f.stack.clear();
    auto* raw = new RemovingHandler;
    raw->cid = original;
    REQUIRE(f.system.registerHandler(makeMeta("Structure"), FeatureSystem::SystemHandlerPtr { raw }));
    Index added;
    {
        UndoStack::OwnerScope owner(&f.stack, "Structure");
        REQUIRE(f.stack.beginScope("preview structures"));
        added = addTriangleComponent(f.mgr, f.stack);
        f.mgr.getComponentOperator(original)->appendPoint({ 3, 3, 3 });
    }
    f.system.invoke("Structure");
    REQUIRE_FALSE(f.stack.scopeActive());
    REQUIRE(UndoStackTestPeer::undoCount(f.stack) == 1);
    REQUIRE_FALSE(f.mgr.findComponent(original));
    REQUIRE(f.stack.undo());
    REQUIRE_FALSE(f.mgr.findComponent(added));
    REQUIRE(f.mgr.findComponent(original)->mesh->vertex_positions_.size() == 3);
    REQUIRE(f.stack.redo());
    REQUIRE(f.mgr.findComponent(added));
    REQUIRE_FALSE(f.mgr.findComponent(original));
}

TEST_CASE("Nested synchronous boundary keeps execute preview before-image", "[FeatureSystem][undo][preview]")
{
    class NestedHandler : public FeatureHandler {
    public:
        UndoStack* stack;
        Index cid;
        std::any execute(FeatureContext& ctx) override
        {
            ModelScope nested(ctx.model, stack, "nested operation");
            ctx.componentOperator(cid)->appendPoint({ 9, 9, 9 });
            return { };
        }
    };
    FeatureUndoFixture f;
    const Index cid = addTriangleComponent(f.mgr, f.stack);
    f.stack.clear();
    auto* raw = new NestedHandler;
    raw->stack = &f.stack;
    raw->cid = cid;
    REQUIRE(f.system.registerHandler(makeMeta("Nested"), FeatureSystem::SystemHandlerPtr { raw }));
    {
        UndoStack::OwnerScope owner(&f.stack, "Nested");
        REQUIRE(f.stack.beginScope("preview"));
        f.mgr.getComponentOperator(cid)->appendPoint({ 5, 5, 5 });
    }
    f.system.invoke("Nested");
    REQUIRE_FALSE(f.stack.scopeActive());
    REQUIRE(UndoStackTestPeer::undoCount(f.stack) == 1);
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_.size() == 5);
    REQUIRE(f.stack.undo());
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_.size() == 3);
    REQUIRE(f.stack.redo());
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_.size() == 5);
}

TEST_CASE("Event exception flushes preview without committing history", "[FeatureSystem][undo][preview]")
{
    FeatureUndoFixture f;
    const Index cid = addTriangleComponent(f.mgr, f.stack);
    f.stack.clear();
    const int notifications = f.obs.component_changed_count;
    FeatureEventGateway gateway(f.bus, f.mgr, &f.stack, "callback");
    auto sub = gateway.subscribe<TestEvent>([&](const TestEvent&) {
        REQUIRE(f.stack.beginScope("partial preview"));
        f.mgr.getComponentOperator(cid)->appendPoint({ 5, 5, 5 });
        throw std::runtime_error("event failure");
    });
    REQUIRE_THROWS_AS(f.bus.publish(TestEvent { }), std::runtime_error);
    REQUIRE(f.stack.scopeActive());
    REQUIRE(UndoStackTestPeer::undoCount(f.stack) == 0);
    REQUIRE(f.obs.component_changed_count == notifications + 1);
    f.stack.cancelScope();
    REQUIRE_FALSE(f.stack.canUndo());
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_.size() == 3);
}

TEST_CASE("Key routing flushes previews without replacing business exceptions", "[FeatureSystem][undo][key]")
{
    class KeyPreviewHandler : public FeatureHandler {
    public:
        void setup(FeatureRegistrar& reg, FeatureContext& ctx) override
        {
            context = &ctx;
            reg.addKeyBinding({ 'P', 0 });
        }
        bool onKeyEvent(const KeyEvent&) override
        {
            context->undo.beginScope("key preview");
            writeVertex(*context, component_id);
            if (fail_route)
                throw std::runtime_error("key failure");
            return true;
        }
        FeatureContext* context { nullptr };
        Index component_id { -1 };
        bool fail_route { false };
    };

    bool fail_route = false;
    bool fail_notification = false;
    SECTION("successful route") { }
    SECTION("route fails") { fail_route = true; }
    SECTION("notification failure preserves route failure")
    {
        fail_route = true;
        fail_notification = true;
    }
    SECTION("notification failure preserves successful consumption") { fail_notification = true; }

    FeatureUndoFixture f;
    const Index cid = addTriangleComponent(f.mgr, f.stack);
    f.stack.clear();
    auto handler = std::make_unique<KeyPreviewHandler>();
    handler->component_id = cid;
    handler->fail_route = fail_route;
    REQUIRE(f.system.registerHandler(makeMeta("Key"), FeatureSystem::SystemHandlerPtr { handler.release() }));
    const int notifications = f.obs.component_changed_count;
    f.obs.fail_component_notification = fail_notification;
    if (fail_route)
        REQUIRE_THROWS_WITH(f.system.dispatchKeyEvent(KeyEvent { 'P', 0, true }), "key failure");
    else
        REQUIRE(f.system.dispatchKeyEvent(KeyEvent { 'P', 0, true }));

    REQUIRE(f.obs.component_changed_count == notifications + 1);
    REQUIRE(f.stack.scopeActive());
    REQUIRE_FALSE(f.stack.inOperation());
    REQUIRE(UndoStackTestPeer::undoCount(f.stack) == 0);
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_[0] == std::array<double, 3> { 5, 5, 5 });
    f.obs.fail_component_notification = false;
    f.system.invoke("Key");
    REQUIRE_FALSE(f.stack.scopeActive());
    REQUIRE(UndoStackTestPeer::undoCount(f.stack) == 1);
    REQUIRE(f.stack.undo());
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_[0] == std::array<double, 3> { 0, 0, 0 });
    REQUIRE_FALSE(f.stack.canUndo());
}

TEST_CASE("Raw key publication failure stays outside routing notification scope", "[FeatureSystem][undo][key]")
{
    FeatureUndoFixture f;
    const Index cid = addTriangleComponent(f.mgr, f.stack);
    f.stack.clear();
    REQUIRE(f.stack.beginScope("pending preview"));
    f.mgr.getComponentOperator(cid)->appendPoint({ 5, 5, 5 });
    const int notifications = f.obs.component_changed_count;
    auto subscription = f.bus.subscribe<KeyEvent>([](const KeyEvent&) {
        throw std::runtime_error("raw key failure");
    });
    REQUIRE_THROWS_WITH(f.system.dispatchKeyEvent(KeyEvent { 'P', 0, true }), "raw key failure");
    REQUIRE(f.obs.component_changed_count == notifications);
    REQUIRE(f.stack.scopeActive());
    REQUIRE(UndoStackTestPeer::undoCount(f.stack) == 0);
    // 原始发布失败未进入路由边界，待通知仍由下一次通知边界消费。
    {
        ModelScope notify(f.mgr, &f.stack, { }, ModelScope::Kind::Notify);
    }
    REQUIRE(f.obs.component_changed_count == notifications + 1);
    f.stack.cancelScope();
    REQUIRE_FALSE(f.stack.canUndo());
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_.size() == 3);
}

TEST_CASE("Managing another feature preserves the active owner's preview", "[FeatureSystem][undo][preview]")
{
    FeatureUndoFixture f;
    const Index cid = addTriangleComponent(f.mgr, f.stack);
    f.stack.clear();
    auto handler = std::make_unique<EventWritingFeatureHandler>();
    handler->component_id = cid;
    REQUIRE(f.system.registerHandler(makeMeta("A"), FeatureSystem::SystemHandlerPtr { handler.release() }));
    REQUIRE(f.system.registerHandler(makeMeta("B"), FeatureSystem::SystemHandlerPtr { std::make_unique<WritingFeatureHandler>().release() }));
    REQUIRE(f.system.setFeatureActive("A"));
    f.bus.publish(TestEvent { });
    REQUIRE(f.stack.scopeActive());
    SECTION("unregister another feature") { f.system.unregisterHandler(makeMeta("B")); }
    SECTION("replace another feature")
    {
        REQUIRE(f.system.registerHandler(makeMeta("B"), FeatureSystem::SystemHandlerPtr { std::make_unique<WritingFeatureHandler>().release() }));
    }
    CHECK(f.stack.scopeActive());
    CHECK(f.mgr.findComponent(cid)->mesh->vertex_positions_[0] == std::array<double, 3> { 5, 5, 5 });
    REQUIRE(f.system.setFeatureActive(""));
    CHECK_FALSE(f.stack.scopeActive());
    CHECK_FALSE(f.stack.canUndo());
    CHECK(f.mgr.findComponent(cid)->mesh->vertex_positions_[0] == std::array<double, 3> { 0, 0, 0 });
}
