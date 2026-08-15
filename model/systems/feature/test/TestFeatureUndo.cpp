/**
 * @file TestFeatureUndo.cpp
 * @brief FeatureSystem undo 集成测试：Auto 边界自动记录 / Manual 插件自控 / 网关两路
 */
#include "EventBus.h"
#include "FeatureContext.h"
#include "FeatureEvents.h"
#include "FeatureHandler.h"
#include "FeatureSystem.h"
#include "ComponentData.h"
#include "ComponentOperator.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "ModelObserver.h"
#include "UndoStack.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <memory>
#include <string>

using namespace systems::feature;

namespace {
struct CountingObserver : ModelObserver {
    int component_changed_count { 0 };

    void notifyModelChanged(Index) override { }
    void notifyModelAdded(Index) override { }
    void notifyModelRemoved(Index) override { }
    void notifyComponentRemoved(Index) override { }
    void notifyComponentChanged(Index) override { ++component_changed_count; }
    void notifyModelNameChanged(Index, const std::string&) override { }
    void notifyGeometryLoadFailed(const std::string&) override { }
};

struct TestEvent { };

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

//! @brief execute 内写模型的功能（Auto/Manual 由元数据决定）
class WritingFeatureHandler : public FeatureHandler {
public:
    std::any execute(FeatureContext& ctx) override
    {
        ++execute_count;
        automatic_mode = ctx.undo.automatic;
        if (component_id >= 0)
            writeVertex(ctx, component_id);
        return {};
    }

    Index component_id { -1 };
    int execute_count { 0 };
    bool automatic_mode { false };
};

//! @brief activate 内经 ctx.events 订阅事件、回调内写模型的功能
class EventWritingFeatureHandler : public FeatureHandler {
public:
    void activate(FeatureContext& ctx) override
    {
        context = &ctx;
        sub = ctx.events.subscribe<TestEvent>([this](const TestEvent&) {
            if (component_id >= 0)
                writeVertex(*context, component_id);
        });
    }

    Index component_id { -1 };
    FeatureContext* context { nullptr };
    core::EventBus::Subscription sub;
};

//! @brief execute 内经 ctx.undo 的 staged 会话写模型的 Manual 功能
class StagedFeatureHandler : public FeatureHandler {
public:
    std::any execute(FeatureContext& ctx) override
    {
        if (component_id < 0)
            return {};
        if (!ctx.undo.beginStaged(label, component_id))
            return {};
        writeVertex(ctx, component_id);
        ctx.undo.commitStaged();
        return {};
    }

    Index component_id { -1 };
    std::string label { "staged操作" };
};

HandlerMetaData makeMeta(const std::string& name, bool undo_manual)
{
    HandlerMetaData meta;
    meta.name = name;
    meta.display_name = "显示_" + name;
    meta.undo_manual = undo_manual;
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
    const Index cid = addTriangleComponent(f.mgr);
    f.stack.clear();

    auto* raw = new WritingFeatureHandler;
    raw->component_id = cid;
    FeatureSystem::SystemHandlerPtr handler { raw };
    REQUIRE(f.system.registerHandler(makeMeta("AutoFeature", false), std::move(handler)));

    f.system.invoke("AutoFeature");
    REQUIRE(raw->execute_count == 1);
    REQUIRE(raw->automatic_mode); // Auto 功能 undo.automatic 为 true
    REQUIRE(f.stack.canUndo());
    REQUIRE(f.stack.undoLabel() == "显示_AutoFeature");

    // 记录可用：undo 恢复写前状态
    f.stack.undo();
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_[0]
        == std::array<double, 3> { 0.0, 0.0, 0.0 });
}

TEST_CASE("FeatureSystem manual feature invoke produces no record but still flushes", "[FeatureSystem][undo]")
{
    FeatureUndoFixture f;
    const Index cid = addTriangleComponent(f.mgr);
    f.stack.clear();
    const int notify_before = f.obs.component_changed_count;

    auto* raw = new WritingFeatureHandler;
    raw->component_id = cid;
    FeatureSystem::SystemHandlerPtr handler { raw };
    REQUIRE(f.system.registerHandler(makeMeta("ManualFeature", true), std::move(handler)));

    f.system.invoke("ManualFeature");
    REQUIRE(raw->execute_count == 1);
    REQUIRE_FALSE(raw->automatic_mode); // Manual 功能 undo.automatic 为 false
    // 无记录（插件未走 staged），但 invoke 边界 flush 照发
    REQUIRE_FALSE(f.stack.canUndo());
    REQUIRE(f.obs.component_changed_count == notify_before + 1);
}

TEST_CASE("FeatureEventGateway wraps callbacks with undo boundary per mode", "[FeatureSystem][undo]")
{
    FeatureUndoFixture f;
    const Index cid = addTriangleComponent(f.mgr);
    f.stack.clear();

    // Auto 功能：网关回调包 beginOperation/commitOperation，事件写模型成一条记录
    auto* auto_raw = new EventWritingFeatureHandler;
    auto_raw->component_id = cid;
    FeatureSystem::SystemHandlerPtr auto_handler { auto_raw };
    REQUIRE(f.system.registerHandler(makeMeta("AutoEvent", false), std::move(auto_handler)));

    f.bus.publish(TestEvent {});
    REQUIRE(f.stack.canUndo());
    REQUIRE(f.stack.undoLabel() == "显示_AutoEvent");

    // Manual 功能：网关回调只 flush，不成记录。先注销 Auto 功能，避免其回调同事件干扰断言
    f.system.unregisterHandler(makeMeta("AutoEvent", false));
    f.stack.clear();
    const int notify_before = f.obs.component_changed_count;
    auto* manual_raw = new EventWritingFeatureHandler;
    manual_raw->component_id = cid;
    FeatureSystem::SystemHandlerPtr manual_handler { manual_raw };
    REQUIRE(f.system.registerHandler(makeMeta("ManualEvent", true), std::move(manual_handler)));

    f.bus.publish(TestEvent {});
    REQUIRE_FALSE(f.stack.canUndo()); // Auto 的记录已被 clear，Manual 不成记录
    REQUIRE(f.obs.component_changed_count == notify_before + 1); // 通知照发
}

TEST_CASE("Auto feature read-only event callback does not cancel an open staged session", "[FeatureSystem][undo]")
{
    FeatureUndoFixture f;
    const Index cid = addTriangleComponent(f.mgr);
    f.stack.clear();

    // 模拟 ScalePreview 场景：Manual 功能已开 staged 会话并写入预览（会话保持打开）
    REQUIRE(f.stack.beginStaged("预览", cid));
    {
        auto op = f.mgr.getComponentOperator(cid);
        REQUIRE(op.has_value());
        op->editableMesh().vertex_positions_[0] = { 5.0, 5.0, 5.0 };
    }

    // Auto 功能的只读事件订阅（如 FeatureDemo 打日志）：经网关包操作边界但无写入
    auto* raw = new EventWritingFeatureHandler; // component_id 默认 -1：只读回调
    FeatureSystem::SystemHandlerPtr handler { raw };
    REQUIRE(f.system.registerHandler(makeMeta("BystanderFeature", false), std::move(handler)));

    f.bus.publish(TestEvent {});
    // 旁观回调不得误杀 staged：会话与预览状态保持，且空边界不成记录
    REQUIRE(f.stack.stagedActive());
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_[0]
        == std::array<double, 3> { 5.0, 5.0, 5.0 });
    REQUIRE(f.stack.undoLabel() == "预览");

    f.stack.cancelStaged(); // 清理会话，避免影响后续断言
}

TEST_CASE("Auto feature event callback that writes cancels staged and records before-image", "[FeatureSystem][undo]")
{
    FeatureUndoFixture f;
    const Index cid = addTriangleComponent(f.mgr);
    f.stack.clear();

    REQUIRE(f.stack.beginStaged("预览", cid));
    {
        auto op = f.mgr.getComponentOperator(cid);
        REQUIRE(op.has_value());
        op->editableMesh().vertex_positions_[0] = { 5.0, 5.0, 5.0 };
    }

    // Auto 功能事件回调真实写模型：隐式 cancel 旧预览，写操作正常成记录
    auto* raw = new EventWritingFeatureHandler;
    raw->component_id = cid;
    FeatureSystem::SystemHandlerPtr handler { raw };
    REQUIRE(f.system.registerHandler(makeMeta("WritingEvent", false), std::move(handler)));

    f.bus.publish(TestEvent {});
    REQUIRE_FALSE(f.stack.stagedActive());
    REQUIRE(f.stack.canUndo());
    REQUIRE(f.stack.undoLabel() == "显示_WritingEvent");

    // before-image 是回滚后的 before₀：undo 后无预览残留
    f.stack.undo();
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_[0]
        == std::array<double, 3> { 0.0, 0.0, 0.0 });
}

TEST_CASE("Manual feature controls recording via ctx.undo staged session", "[FeatureSystem][undo]")
{
    FeatureUndoFixture f;
    const Index cid = addTriangleComponent(f.mgr);
    f.stack.clear();

    auto* raw = new StagedFeatureHandler;
    raw->component_id = cid;
    FeatureSystem::SystemHandlerPtr handler { raw };
    REQUIRE(f.system.registerHandler(makeMeta("StagedFeature", true), std::move(handler)));

    f.system.invoke("StagedFeature");
    REQUIRE(f.stack.canUndo());
    REQUIRE(f.stack.undoLabel() == "staged操作");
    REQUIRE_FALSE(f.stack.stagedActive()); // commitStaged 后会话已关闭

    f.stack.undo();
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_[0]
        == std::array<double, 3> { 0.0, 0.0, 0.0 });
}
