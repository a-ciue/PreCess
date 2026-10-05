#include "ComponentData.h"
#include "EventBus.h"
#include "FeatureContext.h"
#include "FeatureEvents.h"
#include "FeatureSystem.h"
#include "Job.h"
#include "JobRunner.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "ModelObserver.h"
#include "ModelScope.h"
#include "ScalePreviewHandler.h"
#include "UndoStack.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

using namespace systems;
using namespace systems::feature;

namespace {
constexpr const char* kFeatureName = "ScalePreview";
// 参数下标：0 缩放因子（Float），1 预览（Button），2 取消（Button）
constexpr std::size_t kParamScale = 0;
constexpr std::size_t kParamPreview = 1;
constexpr std::size_t kParamCancel = 2;
const std::array<double, 3> kOriginal { 1.0, 2.0, 3.0 };

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

//! @brief 构造单点组件模型并入池，返回 component_id
Index addSingleComponentModel(ModelLayer& model_layer, UndoStack& stack)
{
    ModelScope initialization(model_layer, &stack, { }, ModelScope::Kind::Cleanup);
    auto mesh = std::make_unique<MeshData>();
    mesh->init();
    mesh->vertex_positions_ = { kOriginal };

    auto component = std::make_unique<ComponentData>();
    component->id = -1;
    component->name = "Comp_0";
    component->mesh = std::move(mesh);
    ComponentDatas components;
    components.push_back(std::move(component));

    Index model_id = model_layer.addModel("test_model", std::move(components));
    return model_layer.modelById(model_id)->componentIds().front();
}

//! @brief 构造单三角面组件模型并入池，返回 component_id（单点网格无边可签发，无法断言邻接表有效性）
Index addTriangleComponentModel(ModelLayer& model_layer, UndoStack& stack)
{
    ModelScope initialization(model_layer, &stack, { }, ModelScope::Kind::Cleanup);
    auto mesh = std::make_unique<MeshData>();
    mesh->init();
    mesh->vertex_positions_ = { kOriginal, { 2.0, 0.0, 0.0 }, { 0.0, 2.0, 0.0 } };
    mesh->face_vertices_ = { 0, 1, 2 };
    mesh->face_vertices_offset_ = { 0, 3 };

    auto component = std::make_unique<ComponentData>();
    component->id = -1;
    component->name = "Comp_0";
    component->mesh = std::move(mesh);
    ComponentDatas components;
    components.push_back(std::move(component));

    Index model_id = model_layer.addModel("test_model_triangle", std::move(components));
    return model_layer.modelById(model_id)->componentIds().front();
}

//! @brief 功能元数据：与 ScalePreviewPlugin.json 一致
HandlerMetaData scalePreviewMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = kFeatureName;
    meta_data.display_name = "缩放预览演示";
    return meta_data;
}

std::array<double, 3> firstVertex(ModelLayer& model_layer, Index component_id)
{
    return model_layer.findComponent(component_id)->mesh->vertex_positions_[0];
}

//! @brief 公共夹具：ModelLayer + UndoStack + FeatureSystem + 任务执行器（异步预览）
//! 模型覆盖执行器生存期，宿主拆解先 stop，再退出功能和模型。
struct ScalePreviewFixture {
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<std::function<void()>> queued; //!< 排队的提交闭包（回写段）

    CountingObserver obs;
    ModelLayer mgr { &obs };
    core::EventBus bus;
    UndoStack stack { mgr };
    systems::job::JobRunner runner;
    FeatureSystem system { mgr, bus, &stack };

    ScalePreviewFixture()
        : runner(mgr, &stack, [this](std::function<void()> fn) {
            // 分发器：提交闭包排队，由 settle 在测试主线程执行（= 生产 GUI 执行回写段，
            // 写闸线程亲和依赖此点）
            std::lock_guard lock(mutex);
            queued.push_back(std::move(fn));
            cv.notify_all();
        })
    {
        mgr.setUndoRecorder(&stack);
        system.setJobRunner(&runner);
    }

    ~ScalePreviewFixture() { runner.stop(); } // 与宿主一致：先停任务，再拆模型。

    //! @brief 等计算真正返回，取出尚未消费的 GUI 收尾，确定性控制回写窗口。
    std::function<void()> takeCompletion()
    {
        std::unique_lock lock(mutex);
        REQUIRE(cv.wait_for(lock, std::chrono::seconds(5), [this] { return !queued.empty(); }));
        auto fn = std::move(queued.front());
        queued.erase(queued.begin());
        return fn;
    }

    //! @brief 排空任务：执行排队回写段，直到无在飞任务（预览生效的同步点）
    void settle()
    {
        for (int spins = 0;; ++spins) {
            std::function<void()> fn;
            {
                std::lock_guard lock(mutex);
                if (!queued.empty()) {
                    fn = std::move(queued.front());
                    queued.erase(queued.begin());
                }
            }
            if (fn) {
                fn(); // 回写段在测试主线程执行（= 生产 GUI）
                continue;
            }
            std::unique_lock lock(mutex);
            if (!runner.currentJob())
                return;
            REQUIRE(spins < 500); // 5s 上限（每轮 10ms）
            cv.wait_for(lock, std::chrono::milliseconds(10));
        }
    }

    //! @brief 注册功能并注入活动组件 provider（组件由调用方先行入池并清栈）
    Index setupFeatureOn(Index component_id)
    {
        FeatureSystem::SystemHandlerPtr handler { new ScalePreviewHandler };
        REQUIRE(system.registerHandler(scalePreviewMetaData(), std::move(handler)));
        system.setActiveComponentProvider([component_id]() { return std::optional<Index> { component_id }; });
        return component_id;
    }

    //! @brief 建组件、清栈（入池的结构记录不计入断言）、注册功能并注入活动组件 provider
    Index setupFeature()
    {
        const Index component_id = addSingleComponentModel(mgr, stack);
        stack.clear();
        return setupFeatureOn(component_id);
    }
};
}

TEST_CASE("ScalePreview preview button opens session without record but still flushes", "[ScalePreviewPlugin]")
{
    ScalePreviewFixture f;
    const Index cid = f.setupFeature();
    const int notify_before = f.obs.component_changed_count;

    // 默认因子 1.0：预览不改坐标，但 editableMesh 获取即标脏
    REQUIRE(f.system.setParameter(kFeatureName, kParamPreview, core::ArgObject::create<ArgTypeEnum::Button>(1)));
    f.settle(); // 异步预览：泵到回写段落地（= 生产 GUI 执行）

    // 网关统一边界但本回调无写；层保持打开（通知照发）
    REQUIRE(f.stack.scopeActive());
    REQUIRE(f.obs.component_changed_count == notify_before + 1);
    // canUndo 在层打开时表示"可取消预览"（undo=cancelScope），记录有无须在层
    // 关闭后断言：取消会话后栈仍为空，证明预览写未入栈
    f.stack.cancelScope();
    REQUIRE_FALSE(f.stack.canUndo());
}

TEST_CASE("ScalePreview full cycle: preview, retry with new factor, confirm via menu, undo", "[ScalePreviewPlugin]")
{
    ScalePreviewFixture f;
    const Index cid = f.setupFeature();

    // 层未打开时改因子只更新参数，不改模型
    REQUIRE(f.system.setParameter(kFeatureName, kParamScale, core::ArgObject::create<ArgTypeEnum::Float>(2.0)));
    REQUIRE(firstVertex(f.mgr, cid) == kOriginal);

    // 预览按钮：开层并按 2.0 预览
    REQUIRE(f.system.setParameter(kFeatureName, kParamPreview, core::ArgObject::create<ArgTypeEnum::Button>(1)));
    f.settle(); // 异步预览：泵到回写段落地（= 生产 GUI 执行）
    REQUIRE(f.stack.scopeActive());
    REQUIRE(firstVertex(f.mgr, cid) == std::array<double, 3> { 2.0, 4.0, 6.0 });

    // 预览重试：revertScope 回 before₀ 再按新因子 3.0 重写（绝对因子，非累计 ×6）
    REQUIRE(f.system.setParameter(kFeatureName, kParamScale, core::ArgObject::create<ArgTypeEnum::Float>(3.0)));
    f.settle();
    REQUIRE(firstVertex(f.mgr, cid) == std::array<double, 3> { 3.0, 6.0, 9.0 });
    REQUIRE(f.stack.scopeActive());
    // 预览期不成记录由收尾的"恰一条记录"断言覆盖（层打开时 canUndo 恒 true，
    // 语义为"可取消预览"，不能用于判记录有无）

    // 菜单触发 execute = 确认：before₀ + 当前状态恰记一条
    f.system.invoke(kFeatureName);
    REQUIRE_FALSE(f.stack.scopeActive());
    REQUIRE(f.stack.canUndo());
    REQUIRE(f.stack.undoLabel() == "缩放预览");

    // undo 回原值；栈清空证明恰一条记录
    f.stack.undo();
    REQUIRE(firstVertex(f.mgr, cid) == kOriginal);
    REQUIRE_FALSE(f.stack.canUndo());
}

TEST_CASE("ScalePreview cancel rolls back preview without record", "[ScalePreviewPlugin]")
{
    ScalePreviewFixture f;
    const Index cid = f.setupFeature();

    REQUIRE(f.system.setParameter(kFeatureName, kParamScale, core::ArgObject::create<ArgTypeEnum::Float>(2.0)));
    REQUIRE(f.system.setParameter(kFeatureName, kParamPreview, core::ArgObject::create<ArgTypeEnum::Button>(1)));
    f.settle(); // 异步预览：泵到回写段落地（= 生产 GUI 执行）
    REQUIRE(firstVertex(f.mgr, cid) == std::array<double, 3> { 2.0, 4.0, 6.0 });

    // 取消：回滚到 before₀ 并关闭会话，不成记录
    REQUIRE(f.system.setParameter(kFeatureName, kParamCancel, core::ArgObject::create<ArgTypeEnum::Button>(1)));
    REQUIRE_FALSE(f.stack.scopeActive());
    REQUIRE(firstVertex(f.mgr, cid) == kOriginal);
    REQUIRE_FALSE(f.stack.canUndo());
}

TEST_CASE("ScalePreview teardown cancels an open scope session", "[ScalePreviewPlugin]")
{
    ScalePreviewFixture f;
    const Index cid = f.setupFeature();

    REQUIRE(f.system.setParameter(kFeatureName, kParamScale, core::ArgObject::create<ArgTypeEnum::Float>(2.0)));
    REQUIRE(f.system.setParameter(kFeatureName, kParamPreview, core::ArgObject::create<ArgTypeEnum::Button>(1)));
    f.settle(); // 异步预览：泵到回写段落地（= 生产 GUI 执行）
    REQUIRE(f.stack.scopeActive());

    // 功能注销触发 teardown：层未关由框架在插件回调后兜底关闭（FeatureSystem 退出路径，AGENTS 任务线程规约）
    f.system.unregisterHandler(scalePreviewMetaData());
    REQUIRE_FALSE(f.stack.scopeActive());
    REQUIRE(firstVertex(f.mgr, cid) == kOriginal);
    REQUIRE_FALSE(f.stack.canUndo());
}

TEST_CASE("ScalePreview preview keeps adjacency handle valid (NonTopology kind)", "[ScalePreviewPlugin]")
{
    ScalePreviewFixture f;
    const Index cid = f.setupFeatureOn(addTriangleComponentModel(f.mgr, f.stack));
    f.stack.clear();

    ComponentData* comp = f.mgr.findComponent(cid);
    REQUIRE(comp);
    MeshData& md = *comp->mesh;

    // 预览前签发当轮边表句柄：回归护栏——缩放只改坐标，邻接懒表不得失效
    // （此前 applyPreview 误传 MeshEditKind::Topology，每次预览重试都白重建一次边表）
    auto edge = comp->mesh_adjacency.findEdgeByEndpoints(md, 0, 1);
    REQUIRE(edge.has_value());
    const auto sid = comp->mesh_adjacency.edgeStableId(md, *edge);
    REQUIRE(sid.has_value());

    REQUIRE(f.system.setParameter(kFeatureName, kParamScale, core::ArgObject::create<ArgTypeEnum::Float>(2.0)));
    REQUIRE(f.system.setParameter(kFeatureName, kParamPreview, core::ArgObject::create<ArgTypeEnum::Button>(1)));
    f.settle(); // 异步预览：泵到回写段落地（= 生产 GUI 执行）
    REQUIRE(f.stack.scopeActive());
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_[0] == std::array<double, 3> { 2.0, 4.0, 6.0 });

    // 坐标缩放不动连通性：预览后句柄仍属当轮边表（Topology 标脏会在此失效）
    REQUIRE(comp->mesh_adjacency.edgeStableId(md, *edge) == sid);
}

TEST_CASE("ScalePreview teardown with open scope session is safe", "[ScalePreviewPlugin]")
{
    // 对应程序退出路径：层未关时宿主析构——~FeatureSystem 停用 handler，
    // teardown 后由框架兜底 cancelScope 回滚预览。本用例用作用域模拟宿主的
    // 显式有序拆解（见 QModelManager 析构注释）：FeatureSystem 先析构，
    // UndoStack / ModelLayer 存活，拆解链不崩且预览被回滚
    CountingObserver obs;
    ModelLayer mgr { &obs };
    core::EventBus bus;
    UndoStack stack { mgr };
    mgr.setUndoRecorder(&stack);
    const Index cid = addSingleComponentModel(mgr, stack);
    stack.clear();

    {
        FeatureSystem system { mgr, bus, &stack };
        FeatureSystem::SystemHandlerPtr handler { new ScalePreviewHandler };
        REQUIRE(system.registerHandler(scalePreviewMetaData(), std::move(handler)));
        system.setActiveComponentProvider([cid]() { return std::optional<Index> { cid }; });

        REQUIRE(system.setParameter(kFeatureName, kParamScale, core::ArgObject::create<ArgTypeEnum::Float>(2.0)));
        REQUIRE(system.setParameter(kFeatureName, kParamPreview, core::ArgObject::create<ArgTypeEnum::Button>(1)));
        REQUIRE(stack.scopeActive());
        // 作用域结束：~FeatureSystem → teardown → cancelScope（栈与模型层仍存活）
    }

    REQUIRE_FALSE(stack.scopeActive());
    REQUIRE(firstVertex(mgr, cid) == kOriginal);
    REQUIRE_FALSE(stack.canUndo());
}

TEST_CASE("ScalePreview discards preview when runner not injected", "[ScalePreviewPlugin]")
{
    // 未注入执行器在生产不可达（QModelManager 构造期注入必达）——本用例钉框架契约：
    // runJob 返回空 = 直接丢弃，不计算、不写模型（会话照常开，槽空装配后下笔变更接上）
    CountingObserver obs;
    ModelLayer mgr { &obs };
    core::EventBus bus;
    UndoStack stack { mgr };
    mgr.setUndoRecorder(&stack);
    const Index cid = addSingleComponentModel(mgr, stack);
    stack.clear();

    FeatureSystem system { mgr, bus, &stack };
    FeatureSystem::SystemHandlerPtr handler { new ScalePreviewHandler };
    REQUIRE(system.registerHandler(scalePreviewMetaData(), std::move(handler)));
    system.setActiveComponentProvider([cid]() { return std::optional<Index> { cid }; });

    REQUIRE(system.setParameter(kFeatureName, kParamScale, core::ArgObject::create<ArgTypeEnum::Float>(2.0)));
    REQUIRE(system.setParameter(kFeatureName, kParamPreview, core::ArgObject::create<ArgTypeEnum::Button>(1)));
    REQUIRE(stack.scopeActive()); // 会话照常开（开会在任务提交之前）
    REQUIRE(firstVertex(mgr, cid) == kOriginal); // 丢弃：模型零变化

    // 因子变更同样丢弃（会话与 base 保留，但通道仍不可用）
    REQUIRE(system.setParameter(kFeatureName, kParamScale, core::ArgObject::create<ArgTypeEnum::Float>(3.0)));
    REQUIRE(firstVertex(mgr, cid) == kOriginal);
}

TEST_CASE("ScalePreview discards preview while slot occupied then recovers", "[ScalePreviewPlugin]")
{
    // 单槽被占 = 框架背压：runJob 忙拒绝 → 契约丢弃（不计算、不写模型）；
    // 槽放行后下一笔因子变更自然接上正常异步路径（丢弃不残留、会话未被破坏）
    ScalePreviewFixture f;
    const Index cid = f.setupFeature();
    REQUIRE(f.system.setParameter(kFeatureName, kParamScale, core::ArgObject::create<ArgTypeEnum::Float>(2.0)));

    std::mutex bm;
    std::condition_variable bc;
    bool release = false;
    bool blocker_done = false;
    f.runner.setOnFinished([&](systems::job::Job&) { // 终态回调先于/晚于让槽？——finish 先让槽再触发本回调，
        std::lock_guard lk(bm); // 本标志为真时槽必已空（见 JobRunner::finish 顺序）
        blocker_done = true;
        bc.notify_all();
    });
    auto blocker = f.runner.run("blocker", [&] { return systems::job::JobWork { [&](systems::job::ProgressFn) {
            std::unique_lock lk(bm);
            bc.wait(lk, [&] { return release; }); }, { } }; }, { });
    REQUIRE(blocker != nullptr);

    REQUIRE(f.system.setParameter(kFeatureName, kParamPreview, core::ArgObject::create<ArgTypeEnum::Button>(1)));
    CHECK_FALSE(f.stack.scopeActive()); // 写占用期连开层也拒绝，不能改 undo 归属。
    CHECK(firstVertex(f.mgr, cid) == kOriginal);

    {
        std::lock_guard lk(bm);
        release = true;
    } // 放行外部任务（执行器停机前须排空）
    bc.notify_all();
    f.settle(); // 所属线程消费一次完成通知，计算返回本身不让出槽。
    REQUIRE(blocker_done);

    // 槽已空：下笔因子变更走正常异步路径落地
    REQUIRE(f.system.setParameter(kFeatureName, kParamScale, core::ArgObject::create<ArgTypeEnum::Float>(3.0)));
    REQUIRE(f.system.setParameter(kFeatureName, kParamPreview, core::ArgObject::create<ArgTypeEnum::Button>(2)));
    f.settle();
    REQUIRE(firstVertex(f.mgr, cid) == std::array<double, 3> { 3.0, 6.0, 9.0 });
    REQUIRE(f.stack.scopeActive());
}

TEST_CASE("ScalePreview factor change during in-flight preview lands latest factor", "[ScalePreviewPlugin]")
{
    // 在飞窗口确定性复现：预览任务的提交闭包排队等 GUI 泵（状态 Running，非终态），
    // 此时改因子 → 置 pending 且不追发新任务；回写段收尾复核最新因子（f_eff ≠ computed
    // → 就地按最新值重算）——最后一笔变更必落模型
    ScalePreviewFixture f;
    const Index cid = f.setupFeature();
    REQUIRE(f.system.setParameter(kFeatureName, kParamScale, core::ArgObject::create<ArgTypeEnum::Float>(2.0)));
    REQUIRE(f.system.setParameter(kFeatureName, kParamPreview, core::ArgObject::create<ArgTypeEnum::Button>(1)));
    auto complete = f.takeCompletion(); // 计算已完成，回写仍排队，结果必按 2.0 计算。
    REQUIRE(f.system.setParameter(kFeatureName, kParamScale, core::ArgObject::create<ArgTypeEnum::Float>(3.0)));

    complete();
    f.settle();
    REQUIRE(firstVertex(f.mgr, cid) == std::array<double, 3> { 3.0, 6.0, 9.0 }); // 3.0 生效而非 2.0
    REQUIRE(f.stack.scopeActive()); // 层保持（预览写归层不杀层）
}

TEST_CASE("ScalePreview execute without session scales directly and records undo", "[ScalePreviewPlugin]")
{
    ScalePreviewFixture f;
    const Index cid = f.setupFeature();

    // GUI 复现路径：填入因子（无会话只更新参数，不改模型）→ 直接点执行（未点"预览"）
    REQUIRE(f.system.setParameter(kFeatureName, kParamScale, core::ArgObject::create<ArgTypeEnum::Float>(2.0)));
    REQUIRE(firstVertex(f.mgr, cid) == kOriginal);
    REQUIRE_FALSE(f.stack.scopeActive());

    // 直接执行：同步按因子缩放 + 成一条 undo 记录（原行为 commitScope 空转、无任何反馈）
    f.system.invoke(kFeatureName);
    REQUIRE(firstVertex(f.mgr, cid) == std::array<double, 3> { 2.0, 4.0, 6.0 });
    REQUIRE_FALSE(f.stack.scopeActive()); // 一次性路径不开会话
    REQUIRE(f.stack.canUndo());
    REQUIRE(f.stack.undoLabel() == "缩放预览");

    // undo 恰恢复原值、栈清空 = 恰一条记录
    f.stack.undo();
    REQUIRE(firstVertex(f.mgr, cid) == kOriginal);
    REQUIRE_FALSE(f.stack.canUndo());
}

TEST_CASE("ScalePreview session keeps steps undoable and collapses them at switch-out", "[ScalePreviewPlugin][session]")
{
    ScalePreviewFixture f;
    const Index cid = f.setupFeature();

    // 进入功能 = 开会话（此后本功能成的记录打会话标）
    REQUIRE(f.system.setFeatureActive(kFeatureName));

    // 第一步：无预览会话 → 直接按 2.0 缩放，自成一条记录
    REQUIRE(f.system.setParameter(kFeatureName, kParamScale, core::ArgObject::create<ArgTypeEnum::Float>(2.0)));
    f.system.invoke(kFeatureName);
    REQUIRE(firstVertex(f.mgr, cid) == std::array<double, 3> { 2.0, 4.0, 6.0 });
    REQUIRE(f.stack.undoLabel() == "缩放预览");

    // 第二步：改因子再执行（增量缩放）→ 会话期两条记录彼此独立
    REQUIRE(f.system.setParameter(kFeatureName, kParamScale, core::ArgObject::create<ArgTypeEnum::Float>(3.0)));
    f.system.invoke(kFeatureName);
    REQUIRE(firstVertex(f.mgr, cid) == std::array<double, 3> { 6.0, 12.0, 18.0 });

    // 会话期逐步可撤：一次撤销只退回上一步，redo 再做回来（记录带着会话标回栈顶）
    f.stack.undo();
    REQUIRE(firstVertex(f.mgr, cid) == std::array<double, 3> { 2.0, 4.0, 6.0 });
    f.stack.redo();
    REQUIRE(firstVertex(f.mgr, cid) == std::array<double, 3> { 6.0, 12.0, 18.0 });

    // 退出功能 = 收尾折叠：两步压成一条 → 一步撤回会话前状态
    // （折叠条目用功能显示名；会话期单条记录的名字是层标签"缩放预览"——空边界吸收层标签）
    REQUIRE(f.system.setFeatureActive(""));
    REQUIRE(f.stack.undoLabel() == "缩放预览演示");
    f.stack.undo();
    REQUIRE(firstVertex(f.mgr, cid) == kOriginal);
    REQUIRE_FALSE(f.stack.canUndo()); // 未折叠的话这里还剩一条
}

TEST_CASE("ScalePreview cancellation before queued write discards the completed result", "[ScalePreviewPlugin][operation]")
{
    ScalePreviewFixture f;
    const Index target = f.setupFeature();
    REQUIRE(f.system.setFeatureActive(kFeatureName));
    REQUIRE(f.system.setParameter(kFeatureName, kParamScale, core::ArgObject::create<ArgTypeEnum::Float>(2.0)));
    REQUIRE(f.system.setParameter(kFeatureName, kParamPreview, core::ArgObject::create<ArgTypeEnum::Button>(1)));
    auto job = f.runner.currentJob();
    REQUIRE(job);
    auto complete = f.takeCompletion();
    SECTION("cancel button")
    {
        REQUIRE(f.system.setParameter(kFeatureName, kParamCancel, core::ArgObject::create<ArgTypeEnum::Button>(1)));
    }
    SECTION("feature exit")
    {
        REQUIRE(f.system.setFeatureActive(""));
    }
    REQUIRE(job->isCancellationRequested());
    REQUIRE(f.mgr.writesPending());
    REQUIRE(f.stack.scopeActive());
    complete();
    REQUIRE(job->state() == systems::job::JobState::Cancelled);
    REQUIRE_FALSE(f.mgr.writesPending());
    REQUIRE_FALSE(f.stack.scopeActive());
    REQUIRE(firstVertex(f.mgr, target) == kOriginal);
    REQUIRE_FALSE(f.stack.canUndo());
}
