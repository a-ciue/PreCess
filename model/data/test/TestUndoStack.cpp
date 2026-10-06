/**
 * @file TestUndoStack.cpp
 * @brief UndoStack 单元测试：边界自动记录、结构记录、插件层会话、执行路径完备性
 */
#include "ComponentData.h"
#include "ComponentOperator.h"
#include "GeometryData.h"
#include "MeshData.h"
#include "MeshIDMap.h"
#include "ModelLayer.h"
#include "ModelObserver.h"
#include "ModelScope.h"
#include "UndoStack.h"
#include <functional>

#include <BRepPrimAPI_MakeBox.hxx>
#include <TopoDS_Shape.hxx>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
struct CountingObserver : ModelObserver {
    int component_changed_count { 0 };
    Index last_component_changed { -1 };

    void notifyModelChanged(Index) override { }
    std::function<void()> on_added;
    void notifyModelAdded(Index) override
    {
        if (on_added)
            on_added();
    }
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

//! @brief 构造一个简单三角形面片组件（3 点 1 面）
std::unique_ptr<ComponentData> makeTriangleComponent(const std::string& name)
{
    auto mesh = std::make_unique<MeshData>();
    mesh->init();
    mesh->vertex_positions_ = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
    mesh->face_vertices_ = { 0, 1, 2 };
    mesh->face_vertices_offset_ = { 0, 3 };

    auto c = std::make_unique<ComponentData>();
    c->name = name;
    c->mesh = std::move(mesh);
    return c;
}

//! @brief ModelLayer + UndoStack 挂接的测试夹具
struct UndoFixture {
    CountingObserver obs;
    ModelLayer mgr { &obs };
    UndoStack stack { mgr };

    UndoFixture() { mgr.setUndoRecorder(&stack); }

    //! @brief 入池一个三角形组件，返回 {model_id, component_id}
    std::pair<Index, Index> addTriangle(const std::string& model_name = "test_model")
    {
        std::optional<ModelScope> scope;
        if (!stack.inOperation() && !stack.scopeActive())
            scope.emplace(mgr, &stack, "添加模型");
        ComponentDatas comps;
        comps.push_back(makeTriangleComponent("Comp_0"));
        const Index model_id = mgr.addModel(model_name, std::move(comps));
        return { model_id, mgr.modelById(model_id)->componentIds()[0] };
    }
};

//! @brief 经可写入口改写一个点坐标（获取即标脏）
void writeVertex(ModelLayer& mgr, Index component_id, Index local, std::array<double, 3> pos)
{
    auto op = mgr.getComponentOperator(component_id);
    REQUIRE(op.has_value());
    MeshData& md = op->editableMesh();
    md.vertex_positions_[static_cast<size_t>(local)] = pos;
}

std::array<double, 3> vertexAt(ModelLayer& mgr, Index component_id, Index local)
{
    return mgr.findComponent(component_id)->mesh->vertex_positions_[static_cast<size_t>(local)];
}

//! @brief 经真实结构入口添加几何组件，再经写面赋网格以保证 gid 配套。
Index addGeometryTriangle(ModelLayer& model, Index model_id)
{
    auto component = std::make_unique<ComponentData>();
    component->name = "box with mesh";
    component->geometry = std::make_unique<GeometryData>();
    component->geometry->rootShape = std::make_unique<TopoDS_Shape>(BRepPrimAPI_MakeBox(1, 1, 1).Shape());
    auto owner = model.getModelOperator(model_id);
    REQUIRE(owner);
    const Index id = owner->addGeometryComponent(std::move(component));
    auto target = model.getComponentOperator(id);
    REQUIRE(target);
    auto triangle = makeTriangleComponent("mesh");
    target->replaceMesh(std::move(triangle->mesh));
    return id;
}
}

TEST_CASE("UndoStack records writes within an operation boundary", "[UndoStack]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear(); // addModel 的即时结构记录不计入本用例
    REQUIRE_FALSE(f.stack.canUndo());

    f.stack.beginOperation("移动顶点");
    writeVertex(f.mgr, cid, 0, { 5.0, 5.0, 5.0 });
    f.stack.commitOperation();

    REQUIRE(f.stack.canUndo());
    REQUIRE(f.stack.undoLabel() == "移动顶点");

    // undo 恢复写前状态，且自身即边界（恢复后通知已发）
    const int notify_before = f.obs.component_changed_count;
    f.stack.undo();
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 });
    REQUIRE(f.obs.component_changed_count == notify_before + 1);
    REQUIRE(f.obs.last_component_changed == cid);
    REQUIRE(f.stack.canRedo());
    REQUIRE(f.stack.redoLabel() == "移动顶点");

    // redo 恢复写后状态
    f.stack.redo();
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 5.0, 5.0, 5.0 });
    REQUIRE(f.stack.canUndo());
    REQUIRE_FALSE(f.stack.canRedo());
}

TEST_CASE("UndoStack captures before-image only on first dirty and groups components into one record", "[UndoStack]")
{
    UndoFixture f;
    const auto [model_id0, cid0] = f.addTriangle();
    const auto [model_id1, cid1] = f.addTriangle("test_model_2");
    f.stack.clear();

    f.stack.beginOperation("批量编辑");
    writeVertex(f.mgr, cid0, 0, { 1.0, 0.0, 0.0 });
    writeVertex(f.mgr, cid0, 0, { 2.0, 0.0, 0.0 }); // 同组件二次写：before 仍为首次写前
    writeVertex(f.mgr, cid1, 1, { 3.0, 3.0, 3.0 });
    f.stack.commitOperation();

    // 多组件操作成一条记录，undo 一次整体回滚
    f.stack.undo();
    REQUIRE(vertexAt(f.mgr, cid0, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 });
    REQUIRE(vertexAt(f.mgr, cid1, 1) == std::array<double, 3> { 1.0, 0.0, 0.0 });
    REQUIRE_FALSE(f.stack.canUndo());
}

TEST_CASE("UndoStack discards empty operations", "[UndoStack]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear();

    f.stack.beginOperation("空操作");
    f.stack.commitOperation(); // 无写入：丢弃
    REQUIRE_FALSE(f.stack.canUndo());
}

TEST_CASE("UndoStack records addModel and redo restores original ids", "[UndoStack]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();

    // addModel 不经边界：钩子即时自成记录
    REQUIRE(f.stack.canUndo());
    REQUIRE(f.stack.undoLabel() == "添加模型");

    // undo：模型消失
    f.stack.undo();
    REQUIRE(f.mgr.modelById(model_id) == nullptr);
    REQUIRE(f.mgr.findComponent(cid) == nullptr);

    // redo：原 model_id/component_id 复原，点 gid 原值 reclaim
    f.stack.redo();
    REQUIRE(f.mgr.modelById(model_id) != nullptr);
    ComponentData* c = f.mgr.findComponent(cid);
    REQUIRE(c);
    REQUIRE(c->point_global_ids_.size() == 3);
    for (Index local = 0; local < 3; ++local) {
        auto [gcid, lid] = f.mgr.pointIdMap().getLocal(c->point_global_ids_[static_cast<size_t>(local)]);
        REQUIRE(gcid == cid);
        REQUIRE(lid == local);
    }
}

TEST_CASE("UndoStack undoes removeModel with snapshot restore and gid reclaim", "[UndoStack]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    const std::vector<Index> gids = f.mgr.findComponent(cid)->point_global_ids_;
    f.stack.clear();

    {
        ModelScope scope(f.mgr, &f.stack, "删除模型");
        f.mgr.removeModel(model_id);
    }
    REQUIRE(f.mgr.modelById(model_id) == nullptr);
    REQUIRE(f.stack.canUndo());
    REQUIRE(f.stack.undoLabel() == "删除模型");

    // undo：快照复原（含 gid reclaim）
    f.stack.undo();
    REQUIRE(f.mgr.modelById(model_id) != nullptr);
    ComponentData* c = f.mgr.findComponent(cid);
    REQUIRE(c);
    REQUIRE(c->point_global_ids_ == gids);
    for (Index local = 0; local < 3; ++local) {
        auto [gcid, lid] = f.mgr.pointIdMap().getLocal(gids[static_cast<size_t>(local)]);
        REQUIRE(gcid == cid);
        REQUIRE(lid == local);
    }
}

TEST_CASE("UndoStack suppresses recording during undo/redo", "[UndoStack]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear();

    f.stack.beginOperation("编辑");
    writeVertex(f.mgr, cid, 0, { 5.0, 5.0, 5.0 });
    f.stack.commitOperation();

    // undo 过程不产生新记录（栈深不增、redo 保留）
    f.stack.undo();
    REQUIRE_FALSE(f.stack.canUndo());
    REQUIRE(f.stack.canRedo());

    // redo 过程同样不产生新记录
    f.stack.redo();
    REQUIRE(f.stack.canUndo());
    REQUIRE_FALSE(f.stack.canRedo());
    REQUIRE(f.stack.undoLabel() == "编辑");
}

TEST_CASE("UndoStack drops the oldest record beyond kMaxDepth", "[UndoStack]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear();

    for (int i = 0; i < static_cast<int>(UndoStack::kMaxDepth) + 3; ++i) {
        f.stack.beginOperation("编辑" + std::to_string(i));
        writeVertex(f.mgr, cid, 0, { static_cast<double>(i), 0.0, 0.0 });
        f.stack.commitOperation();
    }

    int depth = 0;
    while (f.stack.canUndo()) {
        f.stack.undo();
        ++depth;
    }
    REQUIRE(depth == static_cast<int>(UndoStack::kMaxDepth));
}

TEST_CASE("UndoStack scope session commit/cancel/revert", "[UndoStack][scope]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear();

    // execute 边界 → 预览写 → 框架收尾：恰一条记录，undo 回到 begin 前。
    // 层的提交须在操作边界内（最外层规则：框架是全局栈唯一入栈者）——
    // 包边界提交后记录标签用层的（空边界吸收层标签：用户确认的是层标签）
    f.stack.beginOperation("预览", true);
    REQUIRE(f.stack.beginScope("预览"));
    REQUIRE(f.stack.scopeActive());
    writeVertex(f.mgr, cid, 0, { 5.0, 5.0, 5.0 }); // 深度==层活跃深度 → 归层
    f.stack.commitOperation();
    REQUIRE_FALSE(f.stack.scopeActive()); // 边界收尾统一入栈
    REQUIRE(f.stack.canUndo());
    REQUIRE(f.stack.undoLabel() == "预览");
    f.stack.undo();
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 });
    REQUIRE_FALSE(f.stack.canUndo());

    // beginOperation → 写 → cancelScope：无记录且状态回滚
    REQUIRE(f.stack.beginScope("预览2"));
    writeVertex(f.mgr, cid, 0, { 7.0, 7.0, 7.0 });
    f.stack.cancelScope();
    REQUIRE_FALSE(f.stack.scopeActive());
    REQUIRE(f.stack.scopeId() == 0);
    REQUIRE_FALSE(f.stack.canUndo());
    REQUIRE(f.stack.redoLabel() == "预览"); // 取消不产生或覆盖 redo，仍是此前正式撤销的条目。
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 });

    // revertScope：回滚但层保持（可再写再 commit）；提交仍须在边界内
    f.stack.beginOperation("预览3", true);
    REQUIRE(f.stack.beginScope("预览3"));
    const auto scope_id = f.stack.scopeId();
    REQUIRE(scope_id != 0);
    writeVertex(f.mgr, cid, 0, { 7.0, 7.0, 7.0 });
    f.stack.revertScope();
    REQUIRE(f.stack.scopeActive());
    REQUIRE(f.stack.scopeId() == scope_id);
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 });
    writeVertex(f.mgr, cid, 0, { 9.0, 9.0, 9.0 });
    f.stack.commitOperation();
    REQUIRE(f.stack.canUndo());
    f.stack.undo();
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 });

    // 另一处请求开层：未完成的旧层是便宜的 → 直接驱逐（回滚关闭）再开新层，不抛异常
    REQUIRE(f.stack.beginScope("预览4"));
    writeVertex(f.mgr, cid, 0, { 9.0, 9.0, 9.0 }); // 旧层的预览写
    REQUIRE(f.stack.beginScope("预览5")); // 新请求即"旧层被放弃"的证明
    REQUIRE(f.stack.scopeActive());
    REQUIRE(f.stack.undoLabel() == "预览5"); // 新层顶上，标签换成新的
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 }); // 旧层预览已回滚
    f.stack.cancelScope(); // 关掉新层收尾（不留残层）
    REQUIRE_FALSE(f.stack.canUndo()); // 全程未入栈（旧层回滚、新层空转）
}

TEST_CASE("UndoStack undo during scope session cancels it without touching global stack", "[UndoStack][scope]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear();

    // 先制造一条全局记录
    f.stack.beginOperation("前置编辑");
    writeVertex(f.mgr, cid, 1, { 4.0, 4.0, 4.0 });
    f.stack.commitOperation();

    REQUIRE(f.stack.beginScope("预览"));
    writeVertex(f.mgr, cid, 0, { 5.0, 5.0, 5.0 });

    // 层打开时 undo = cancelScope：恢复 before₀ 并关闭层，全局栈记录不动
    f.stack.undo();
    REQUIRE_FALSE(f.stack.scopeActive());
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 });
    REQUIRE(f.stack.canUndo());
    REQUIRE(f.stack.undoLabel() == "前置编辑");
}

TEST_CASE("UndoStack read-only operation boundary does not cancel an open scope session", "[UndoStack][scope]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear();

    REQUIRE(f.stack.beginScope("预览"));
    writeVertex(f.mgr, cid, 0, { 5.0, 5.0, 5.0 });

    // 纯旁观回调（只读事件只建立通知作用域）：不得误杀进行中的预览
    {
        UndoStack::PreviewAccess access(&f.stack);
        ModelScope notification(f.mgr, nullptr, { }, ModelScope::Kind::Notify);
        REQUIRE(vertexAt(f.mgr, cid, 0)[0] == 5.0);
    }
    REQUIRE(f.stack.scopeActive());
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 5.0, 5.0, 5.0 });
    REQUIRE(f.stack.undoLabel() == "预览"); // 空操作丢弃，无新记录

    // 会话仍可正常收尾
    f.stack.cancelScope();
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 });
}

TEST_CASE("UndoStack real write inside a boundary implicitly cancels an open scope session", "[UndoStack][scope]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear();

    REQUIRE(f.stack.beginScope("旧预览"));
    writeVertex(f.mgr, cid, 0, { 5.0, 5.0, 5.0 });

    // 正式入口先回滚旧预览（模拟切换算法），再写入并成记录
    f.stack.beginOperation("新操作");
    REQUIRE_FALSE(f.stack.scopeActive()); // 正式入口先回滚，写钩子不改变目标生命周期
    writeVertex(f.mgr, cid, 1, { 6.0, 6.0, 6.0 });
    REQUIRE_FALSE(f.stack.scopeActive());
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 }); // before₀ 已恢复
    f.stack.commitOperation();
    REQUIRE(f.stack.canUndo());
    REQUIRE(f.stack.undoLabel() == "新操作");

    // undo 一步回到 before₀（before-image 是恢复后的状态，不含旧预览残留）
    f.stack.undo();
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 });
    REQUIRE(vertexAt(f.mgr, cid, 1) == std::array<double, 3> { 1.0, 0.0, 0.0 });

    // 旧功能后续层调用发现层已关：空转容忍不崩
    f.stack.redo();
    f.stack.cancelScope();
    f.stack.revertScope();
    REQUIRE(f.stack.undoLabel() == "新操作");
}

TEST_CASE("UndoStack write after implicit cancel still notifies at boundary flush", "[UndoStack][scope]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear();

    REQUIRE(f.stack.beginScope("旧预览"));
    writeVertex(f.mgr, cid, 0, { 5.0, 5.0, 5.0 });
    f.mgr.flushNotifications(); // 清掉预览写的待通知，只观察后续通知

    // 正式入口 cancelScope 的恢复通知与随后操作的写入通知各自完整
    const int notify_before = f.obs.component_changed_count;
    f.stack.beginOperation("新操作");
    writeVertex(f.mgr, cid, 1, { 6.0, 6.0, 6.0 });
    f.stack.commitOperation();
    f.mgr.flushNotifications(); // 操作边界收尾 flush
    // 恢复 before₀ 一次 + 本次写入一次（去重前各自入过待通知集合）
    REQUIRE(f.obs.component_changed_count == notify_before + 2);
}

TEST_CASE("UndoStack structural operation inside an open scope belongs to the scope", "[UndoStack][scope]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear();

    REQUIRE(f.stack.beginScope("预览"));
    writeVertex(f.mgr, cid, 0, { 5.0, 5.0, 5.0 });

    // 层开着期间的结构操作（无入口边界越过层，如层自己删除/新增模型）：归层而非即时成记录
    f.mgr.removeModel(model_id);
    REQUIRE(f.stack.scopeActive());
    // 没有"删除模型"记录：此刻的撤销入口是层本身（undoLabel 显示层标签）
    REQUIRE(f.stack.undoLabel() == "预览");

    // cancelScope 连同结构一起回滚：模型与预览写都退回开层时状态
    f.stack.cancelScope();
    REQUIRE_FALSE(f.stack.scopeActive());
    REQUIRE_FALSE(f.stack.canUndo()); // 全程没成记录（结构归层，随层一起丢）
    REQUIRE(f.mgr.modelById(model_id) != nullptr);
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 });
}

TEST_CASE("UndoStack structural operation crossing an entry boundary evicts the open scope first", "[UndoStack][scope]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear();

    REQUIRE(f.stack.beginScope("预览"));
    writeVertex(f.mgr, cid, 0, { 5.0, 5.0, 5.0 });

    // 正式入口先回滚预览，结构修改只做捕获，不再隐式回滚。
    f.stack.beginOperation("新操作");
    f.mgr.removeComponent(cid);
    REQUIRE_FALSE(f.stack.scopeActive());
    f.stack.commitOperation();
    REQUIRE(f.stack.canUndo());
    REQUIRE(f.stack.undoLabel() == "新操作");

    // undo 恢复的是回滚后的状态（预览已先回滚），不含预览残留
    f.stack.undo();
    REQUIRE(f.mgr.findComponent(cid) != nullptr);
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 });
}

TEST_CASE("UndoStack scope-created model removed across a boundary rolls back cleanly", "[UndoStack][scope]")
{
    UndoFixture f;
    f.addTriangle();
    f.stack.clear();

    // 层内新增的模型归层（结构捕获）
    REQUIRE(f.stack.beginScope("预览"));
    ComponentDatas comps;
    comps.push_back(makeTriangleComponent("Gen_0"));
    const Index gen_id = f.mgr.addModel("generated", std::move(comps));
    REQUIRE(f.stack.scopeActive());

    // 越界删除该模型：驱逐层 → 层内新增的模型被回滚删掉 → 外层删除扑空收工
    // （重查后无对象可删：不留悬垂迭代器，也不成"删除"记录——它本就是层的产物）
    f.stack.beginOperation("新操作");
    REQUIRE_THROWS_AS(f.mgr.removeModel(gen_id), std::runtime_error);
    REQUIRE_FALSE(f.stack.scopeActive());
    REQUIRE(f.mgr.modelById(gen_id) == nullptr);
    f.stack.commitOperation();
    REQUIRE_FALSE(f.stack.canUndo()); // 边界内无其它改动 → 空操作丢弃
}

TEST_CASE("UndoStack scope rolls back writes to every component it touched", "[UndoStack][scope]")
{
    UndoFixture f;
    const auto [m0, c0] = f.addTriangle("model_0");
    const auto [m1, c1] = f.addTriangle("model_1");
    f.stack.clear();

    f.stack.beginOperation("外层");
    REQUIRE(f.stack.beginScope("预览"));
    writeVertex(f.mgr, c0, 0, { 5.0, 5.0, 5.0 });
    writeVertex(f.mgr, c1, 1, { 6.0, 6.0, 6.0 });
    f.stack.cancelScope(); // 层不指定目标组件：层内写过的每个组件都回滚
    REQUIRE(vertexAt(f.mgr, c0, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 });
    REQUIRE(vertexAt(f.mgr, c1, 1) == std::array<double, 3> { 1.0, 0.0, 0.0 });

    f.stack.commitOperation();
    REQUIRE_FALSE(f.stack.canUndo()); // 层内改动全部回滚 → 边界空操作
}

TEST_CASE("UndoStack session folds its own top contiguous records into one entry", "[UndoStack][session]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear();

    // 会话期每步各自成记录、可逐步撤销；记录要打会话标须与会话同一所有者
    // （生产路径：功能入口压入 OwnerScope，会话由 FeatureSystem 开在同名所有者下）
    UndoStack::OwnerScope owner(&f.stack, "feat");
    f.stack.beginSession("feat", "功能会话");
    for (int i = 1; i <= 3; ++i) {
        const std::string step = "步" + std::to_string(i);
        f.stack.beginOperation(step);
        writeVertex(f.mgr, cid, 0, { static_cast<double>(i), 0.0, 0.0 });
        f.stack.commitOperation();
        REQUIRE(f.stack.undoLabel() == step); // 会话不锁逐步撤销
    }

    // 所有者不符的收尾请求：warn 空转，不折叠
    f.stack.endSession("别人");
    REQUIRE(f.stack.undoLabel() == "步3");

    // 收尾：栈顶连续的同标记录折叠成一条，名字用会话名
    f.stack.endSession("feat");
    REQUIRE(f.stack.undoLabel() == "功能会话");
    f.stack.undo(); // 一步撤完整个会话
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 });
    REQUIRE_FALSE(f.stack.canUndo());

    // 折叠只作用于栈顶连续段：夹在会话记录之间的旁路记录（别的所有者）不被卷进来
    f.stack.clear();
    f.stack.beginSession("feat", "功能会话");
    f.stack.beginOperation("会话一");
    writeVertex(f.mgr, cid, 0, { 1.0, 0.0, 0.0 });
    f.stack.commitOperation();
    {
        UndoStack::OwnerScope other(&f.stack, std::string { }); // 无归属 = 旁路操作
        f.stack.beginOperation("旁路");
        writeVertex(f.mgr, cid, 1, { 9.0, 9.0, 9.0 });
        f.stack.commitOperation();
    }
    f.stack.beginOperation("会话二");
    writeVertex(f.mgr, cid, 0, { 2.0, 0.0, 0.0 });
    f.stack.commitOperation();

    f.stack.endSession("feat");
    REQUIRE(f.stack.undoLabel() == "会话二"); // 旁路之上的会话记录单独成条（太短，不与更早的合并）
    f.stack.undo();
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 1.0, 0.0, 0.0 }); // 会话一的改动仍在栈里
    REQUIRE(f.stack.undoLabel() == "旁路");
    f.stack.undo();
    REQUIRE(vertexAt(f.mgr, cid, 1) == std::array<double, 3> { 1.0, 0.0, 0.0 });
}

TEST_CASE("UndoStack undo during a session steps back its own step, foreign record still undone", "[UndoStack][session]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear();

    // 会话期每步各自成记录（记录要打会话标须与会话同一所有者）
    UndoStack::OwnerScope owner(&f.stack, "feat");
    f.stack.beginSession("feat", "功能会话");
    for (int i = 1; i <= 2; ++i) {
        const std::string step = "步" + std::to_string(i);
        f.stack.beginOperation(step);
        writeVertex(f.mgr, cid, 0, { static_cast<double>(i), 0.0, 0.0 });
        f.stack.commitOperation();
    }

    // 撤销只走统一入口：会话期 undo() 回退的是本会话的上一步（框架代功能调用会话步回退，
    // 插件面不暴露撤销 API）
    f.stack.undo();
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 1.0, 0.0, 0.0 });
    REQUIRE(f.stack.undoLabel() == "步1");
    f.stack.undo();
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 });
    REQUIRE_FALSE(f.stack.canUndo());

    // 栈顶是别人的记录（无归属）→ 交回通用撤销路径照常撤它，不被会话步回退吞掉
    {
        UndoStack::OwnerScope other(&f.stack, std::string { });
        f.stack.beginOperation("旁路");
        writeVertex(f.mgr, cid, 1, { 9.0, 9.0, 9.0 });
        f.stack.commitOperation();
    }
    f.stack.undo();
    REQUIRE(vertexAt(f.mgr, cid, 1) == std::array<double, 3> { 1.0, 0.0, 0.0 });
    REQUIRE_FALSE(f.stack.canUndo());
}

TEST_CASE("UndoStack session folds overlapping components without crossing foreign records", "[UndoStack][session]")
{
    bool with_foreign_record = false;
    SECTION("one continuous session") { }
    SECTION("foreign operation separates earlier session records") { with_foreign_record = true; }

    UndoFixture f;
    const auto [m0, c0] = f.addTriangle("model_0");
    const auto [m1, c1] = f.addTriangle("model_1");
    const auto [m2, c2] = f.addTriangle("model_2");
    f.stack.clear();
    UndoStack::OwnerScope owner(&f.stack, "feat");
    f.stack.beginSession("feat", "session");
    {
        ModelScope scope(f.mgr, &f.stack, "earlier");
        writeVertex(f.mgr, c0, 0, { 1, 0, 0 });
        writeVertex(f.mgr, c1, 1, { 2, 0, 0 });
    }
    if (with_foreign_record) {
        ModelScope scope(f.mgr, &f.stack, "foreign", ModelScope::Kind::Command, "other");
        writeVertex(f.mgr, c0, 1, { 9, 0, 0 });
        writeVertex(f.mgr, c2, 2, { 0, 9, 0 });
    }
    {
        ModelScope scope(f.mgr, &f.stack, "step 1");
        writeVertex(f.mgr, c0, 0, { 3, 0, 0 });
        writeVertex(f.mgr, c1, 1, { 4, 0, 0 });
    }
    {
        ModelScope scope(f.mgr, &f.stack, "step 2");
        writeVertex(f.mgr, c1, 1, { 5, 0, 0 });
        writeVertex(f.mgr, c2, 2, { 0, 6, 0 });
    }
    {
        ModelScope scope(f.mgr, &f.stack, "step 3");
        writeVertex(f.mgr, c0, 0, { 7, 0, 0 });
        writeVertex(f.mgr, c2, 2, { 0, 8, 0 });
    }

    f.stack.endSession("feat");
    REQUIRE(f.stack.undoLabel() == "session");
    REQUIRE(f.stack.undo());
    REQUIRE(vertexAt(f.mgr, c0, 0) == std::array<double, 3> { with_foreign_record ? 1.0 : 0.0, 0, 0 });
    REQUIRE(vertexAt(f.mgr, c1, 1) == std::array<double, 3> { with_foreign_record ? 2.0 : 1.0, 0, 0 });
    REQUIRE(vertexAt(f.mgr, c2, 2) == std::array<double, 3> { 0, with_foreign_record ? 9.0 : 1.0, 0 });
    if (with_foreign_record) {
        REQUIRE(vertexAt(f.mgr, c0, 1) == std::array<double, 3> { 9, 0, 0 });
        REQUIRE(f.stack.undoLabel() == "foreign");
        REQUIRE(f.stack.undo());
        REQUIRE(vertexAt(f.mgr, c0, 0) == std::array<double, 3> { 1, 0, 0 });
        REQUIRE(vertexAt(f.mgr, c0, 1) == std::array<double, 3> { 1, 0, 0 });
        REQUIRE(vertexAt(f.mgr, c2, 2) == std::array<double, 3> { 0, 1, 0 });
        REQUIRE(f.stack.undoLabel() == "earlier");
        REQUIRE(f.stack.undo());
        REQUIRE(vertexAt(f.mgr, c0, 0) == std::array<double, 3> { 0, 0, 0 });
        REQUIRE(vertexAt(f.mgr, c1, 1) == std::array<double, 3> { 1, 0, 0 });
    }
    REQUIRE_FALSE(f.stack.canUndo());
    if (with_foreign_record) {
        REQUIRE(f.stack.redo());
        REQUIRE(f.stack.redo());
    }
    REQUIRE(f.stack.redo());
    REQUIRE(vertexAt(f.mgr, c0, 0) == std::array<double, 3> { 7, 0, 0 });
    REQUIRE(vertexAt(f.mgr, c1, 1) == std::array<double, 3> { 5, 0, 0 });
    REQUIRE(vertexAt(f.mgr, c2, 2) == std::array<double, 3> { 0, 8, 0 });
    REQUIRE(vertexAt(f.mgr, c0, 1) == std::array<double, 3> { with_foreign_record ? 9.0 : 1.0, 0, 0 });
    REQUIRE_FALSE(f.stack.canRedo());
}

namespace {
//! @brief VTK_TETRA 的单元类型编号（model 层不链接 VTK，以字面量给出，值见 vtkCellType.h）
constexpr unsigned char kCellTypeTetra = 10;

/**
 * @brief 构造带 1 个四面体体单元的替代网格
 *
 * 模拟 TetGen「替换当前模型」的产物：2D 面网格 + 3D 体网格（TetGenLibHandler::execute
 * 的替换分支即产出此类网格）。
 */
std::unique_ptr<MeshData> makeTetraMesh()
{
    auto mesh = std::make_unique<MeshData>();
    mesh->init();
    mesh->vertex_positions_ = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
    mesh->vertex_count_ = 4;
    mesh->face_vertices_ = { 0, 1, 2 };
    mesh->face_vertices_offset_ = { 0, 3 };
    mesh->solid_types_.push_back(kCellTypeTetra);
    mesh->solid_vertices_ = { 0, 1, 2, 3 };
    mesh->solid_vertices_offset_ = { 0, 4 };
    return mesh;
}
} // namespace

TEST_CASE("UndoStack undoes replaceMesh back to the pre-replacement mesh", "[UndoStack][replaceMesh]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear();

    // 替换前：纯 2D 面网格，无体单元
    REQUIRE(f.mgr.findComponent(cid)->mesh->solid_types_.empty());

    // 模拟 TetGen「替换当前模型」：2D 面网格替换为 2D 面 + 3D 体网格
    f.stack.beginOperation("TetGen 体网格剖分");
    {
        auto op = f.mgr.getComponentOperator(cid);
        REQUIRE(op.has_value());
        op->replaceMesh(makeTetraMesh());
    }
    f.stack.commitOperation();

    // 替换已生效：出现 3D 体单元，点数增到 4
    REQUIRE(f.mgr.findComponent(cid)->mesh->solid_types_.size() == 1);
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_.size() == 4);
    REQUIRE(f.stack.canUndo());

    // 撤销须退回替换前的纯 2D 网格
    // 回归点：若标脏晚于网格替换，before-image 会克隆到 2D+3D，撤销后 3D 无法消除
    f.stack.undo();
    MeshData* undone = f.mgr.findComponent(cid)->mesh.get();
    REQUIRE(undone);
    REQUIRE(undone->solid_types_.empty());
    REQUIRE(undone->solid_vertices_.empty());
    REQUIRE(undone->vertex_positions_.size() == 3);
    REQUIRE(undone->face_vertices_ == std::vector<Index> { 0, 1, 2 });

    // 重做：3D 体网格回来
    f.stack.redo();
    MeshData* redone = f.mgr.findComponent(cid)->mesh.get();
    REQUIRE(redone->solid_types_.size() == 1);
    REQUIRE(redone->vertex_positions_.size() == 4);
}

TEST_CASE("UndoStack undoes removeMesh and restores the mesh with gids", "[UndoStack][removeMesh]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear();

    const std::vector<Index> gids_before = f.mgr.findComponent(cid)->point_global_ids_;
    REQUIRE(gids_before.size() == 3);

    f.stack.beginOperation("移除网格");
    {
        auto op = f.mgr.getComponentOperator(cid);
        REQUIRE(op.has_value());
        op->removeMesh();
    }
    f.stack.commitOperation();

    REQUIRE(f.mgr.findComponent(cid)->mesh == nullptr);
    REQUIRE(f.stack.canUndo());

    // 撤销须恢复网格与 gid 伴生表
    // 回归点：若标脏晚于 mesh 清空，before-image 已无网格，撤销恢复不回
    f.stack.undo();
    ComponentData* restored = f.mgr.findComponent(cid);
    REQUIRE(restored->mesh != nullptr);
    REQUIRE(restored->mesh->vertex_positions_.size() == 3);
    REQUIRE(restored->mesh->face_vertices_ == std::vector<Index> { 0, 1, 2 });
    REQUIRE(restored->point_global_ids_ == gids_before);

    // gid 已按原值 reclaim：全局映射回指本组件
    for (Index local = 0; local < static_cast<Index>(gids_before.size()); ++local) {
        const auto [mapped_cid, mapped_local]
            = f.mgr.pointIdMap().getLocal(gids_before[static_cast<size_t>(local)]);
        REQUIRE(mapped_cid == cid);
        REQUIRE(mapped_local == local);
    }

    // 重做：网格再次移除
    f.stack.redo();
    REQUIRE(f.mgr.findComponent(cid)->mesh == nullptr);
}

TEST_CASE("UndoStack undo after appendPoint and appendFace leaves no residual elements", "[UndoStack][append]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear();

    f.stack.beginOperation("加点加面");
    {
        auto op = f.mgr.getComponentOperator(cid);
        REQUIRE(op.has_value());
        op->appendPoint({ 0.0, 0.0, 1.0 });
        op->appendFace({ 0, 1, 3 }); // 引用刚追加的局部点 3
    }
    f.stack.commitOperation();

    MeshData* grown = f.mgr.findComponent(cid)->mesh.get();
    REQUIRE(grown->vertex_positions_.size() == 4);
    REQUIRE(grown->face_vertices_offset_.size() == 3); // 2 个面单元

    // 撤销须完全回到加之前：不留新点、新面
    // 回归点：若标脏晚于写入，before-image 已含新点/新面，撤销后残留
    f.stack.undo();
    MeshData* undone = f.mgr.findComponent(cid)->mesh.get();
    REQUIRE(undone->vertex_positions_.size() == 3);
    REQUIRE(undone->vertex_count_ == 3);
    REQUIRE(undone->face_vertices_ == std::vector<Index> { 0, 1, 2 });
    REQUIRE(undone->face_vertices_offset_ == std::vector<Index> { 0, 3 });
    REQUIRE(f.mgr.findComponent(cid)->point_global_ids_.size() == 3);
}

TEST_CASE("materializeEdge idempotent path does not push an empty undo record", "[UndoStack][materializeEdge]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear();

    // 首次物化：真实写入，入栈一条记录
    f.stack.beginOperation("物化边");
    {
        auto op = f.mgr.getComponentOperator(cid);
        REQUIRE(op.has_value());
        op->materializeEdge(0, 1);
    }
    f.stack.commitOperation();
    REQUIRE(f.stack.canUndo());
    REQUIRE(f.mgr.findComponent(cid)->mesh->edge_vertices_.size() == 2);

    // 二次物化同一条边：幂等早退，不应产生新记录
    // 回归点：若标脏提到幂等早退之前，未改数据却捕获 before-image，会入栈空记录
    REQUIRE(f.stack.undoLabel().has_value());
    const std::string label_before = *f.stack.undoLabel();
    f.stack.beginOperation("物化边（幂等）");
    {
        auto op = f.mgr.getComponentOperator(cid);
        REQUIRE(op.has_value());
        op->materializeEdge(0, 1);
    }
    f.stack.commitOperation();

    REQUIRE(*f.stack.undoLabel() == label_before); // 空操作已丢弃，栈顶仍是首次记录
    REQUIRE(f.mgr.findComponent(cid)->mesh->edge_vertices_.size() == 2); // 未重复追加
}

// —— 临界区守卫：撤销/重做与开记录的重入拒绝（提交门在 JobRunner，不在本文件）——

TEST_CASE("G1: undo/redo refused inside operation boundary with zero side effects", "[UndoStack][G1]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear(); // addModel 的即时结构记录不计入本用例

    // 两条已完成记录，制造 redo 残留供拒绝断言
    f.stack.beginOperation("操作A");
    writeVertex(f.mgr, cid, 0, { 1.0, 1.0, 1.0 });
    f.stack.commitOperation();
    f.stack.beginOperation("操作B");
    writeVertex(f.mgr, cid, 0, { 2.0, 2.0, 2.0 });
    f.stack.commitOperation();
    f.stack.undo(); // 弹 B → redo_ 有 "操作B"
    REQUIRE(f.stack.redoLabel() == "操作B");

    // 边界（操作C）开着时 undo/redo → 拒绝：栈、模型、redo 残留、任务联动全部不动
    f.stack.beginOperation("操作C");
    writeVertex(f.mgr, cid, 0, { 3.0, 3.0, 3.0 });
    f.stack.undo();
    f.stack.redo();
    REQUIRE(f.stack.undoLabel() == "操作A");
    REQUIRE(f.stack.redoLabel() == "操作B");
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 3.0, 3.0, 3.0 });

    // 被拒的 undo 不破坏边界配对：边界正常收尾成一条，联动恢复
    f.stack.commitOperation();
    REQUIRE(f.stack.undoLabel() == "操作C");
    f.stack.undo();
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 1.0, 1.0, 1.0 });
    REQUIRE(f.stack.redoLabel() == "操作C");
}

TEST_CASE("G2: restore observer cannot enter an undo operation", "[UndoStack][G2]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear();
    {
        ModelScope scope(f.mgr, &f.stack, "删除模型");
        f.mgr.removeModel(model_id);
    }
    bool observed = false;
    bool body_entered = false;
    f.obs.on_added = [&] {
        observed = true;
        REQUIRE_FALSE(f.stack.undo());
        REQUIRE_FALSE(f.stack.redo());
        REQUIRE_THROWS_AS(([&] {
            ModelScope nested(f.mgr, &f.stack, "观察者插队");
            body_entered = true;
        })(),
            ModelOperationBusy);
        REQUIRE_FALSE(f.stack.inOperation());
    };
    REQUIRE(f.stack.undo());
    REQUIRE(observed);
    REQUIRE_FALSE(body_entered);
    REQUIRE(f.mgr.findComponent(cid));
    REQUIRE_FALSE(f.stack.canUndo());
    REQUIRE(f.stack.redoLabel() == "删除模型");
    REQUIRE(f.stack.redo());
    REQUIRE_FALSE(f.mgr.modelById(model_id));
    REQUIRE(f.stack.undo());
}

TEST_CASE("G2: nested beginOperation outside restore still nests and merges into outer record", "[UndoStack][G2]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear();

    // 回归：守卫只拒绝恢复中的 begin——正常嵌套（直呼归并）不受影响
    f.stack.beginOperation("外层");
    writeVertex(f.mgr, cid, 0, { 1.0, 1.0, 1.0 });
    f.stack.beginOperation("内层");
    writeVertex(f.mgr, cid, 1, { 2.0, 2.0, 2.0 });
    f.stack.commitOperation();
    f.stack.commitOperation();

    REQUIRE(f.stack.undoLabel() == "外层"); // 一次成一条，最外层标签
    f.stack.undo();
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 });
    REQUIRE(vertexAt(f.mgr, cid, 1) == std::array<double, 3> { 1.0, 0.0, 0.0 });
    REQUIRE_FALSE(f.stack.canUndo());
}

TEST_CASE("UndoStack execute absorbs preview without plugin confirmation", "[UndoStack][scope]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear();
    REQUIRE(f.stack.beginScope("预览"));
    writeVertex(f.mgr, cid, 0, { 5.0, 5.0, 5.0 });
    REQUIRE_FALSE(f.stack.inOperation());
    {
        ModelScope execute(f.mgr, &f.stack, "执行", ModelScope::Kind::Execute);
    }
    REQUIRE_FALSE(f.stack.scopeActive());
    REQUIRE(f.stack.undoLabel() == "预览");
    REQUIRE(f.stack.undo());
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 });
    REQUIRE_FALSE(f.stack.canUndo());
    REQUIRE(f.stack.redo());
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 5.0, 5.0, 5.0 });
}

TEST_CASE("Formal undo and execute preview revert keep distinct rollback origins", "[UndoStack][preview]")
{
    UndoFixture f;
    const auto [mid, cid] = f.addTriangle();
    f.stack.clear();
    {
        ModelScope execute(f.mgr, &f.stack, "execute", ModelScope::Kind::Execute, "feature");
        f.mgr.getComponentOperator(cid)->appendPoint({ 3, 0, 0 });
        REQUIRE(f.stack.beginScope("preview"));
        f.mgr.getComponentOperator(cid)->appendPoint({ 4, 0, 0 });
        f.stack.revertScope();
        REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_.size() == 4);
        f.mgr.getComponentOperator(cid)->appendPoint({ 5, 0, 0 });
    }
    REQUIRE_FALSE(f.stack.scopeActive());
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_.size() == 5);
    REQUIRE(f.stack.undo());
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_.size() == 3);
    REQUIRE(f.stack.redo());
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_.size() == 5);
}

TEST_CASE("Execute absorbs overlapping preview components with their earliest before images", "[UndoStack][preview]")
{
    UndoFixture f;
    const auto [m0, c0] = f.addTriangle("model_0");
    const auto [m1, c1] = f.addTriangle("model_1");
    const auto [m2, c2] = f.addTriangle("model_2");
    f.stack.clear();
    {
        ModelScope execute(f.mgr, &f.stack, "execute", ModelScope::Kind::Execute, "feature");
        writeVertex(f.mgr, c0, 0, { 1, 0, 0 });
        writeVertex(f.mgr, c1, 1, { 2, 0, 0 });
        REQUIRE(f.stack.beginScope("preview"));
        writeVertex(f.mgr, c0, 0, { 3, 0, 0 });
        writeVertex(f.mgr, c2, 2, { 0, 4, 0 });
    }
    REQUIRE_FALSE(f.stack.scopeActive());
    REQUIRE(f.stack.undoLabel() == "execute");
    REQUIRE(f.stack.undo());
    REQUIRE(vertexAt(f.mgr, c0, 0) == std::array<double, 3> { 0, 0, 0 });
    REQUIRE(vertexAt(f.mgr, c1, 1) == std::array<double, 3> { 1, 0, 0 });
    REQUIRE(vertexAt(f.mgr, c2, 2) == std::array<double, 3> { 0, 1, 0 });
    REQUIRE_FALSE(f.stack.canUndo());
    REQUIRE(f.stack.redo());
    REQUIRE(vertexAt(f.mgr, c0, 0) == std::array<double, 3> { 3, 0, 0 });
    REQUIRE(vertexAt(f.mgr, c1, 1) == std::array<double, 3> { 2, 0, 0 });
    REQUIRE(vertexAt(f.mgr, c2, 2) == std::array<double, 3> { 0, 4, 0 });
    REQUIRE_FALSE(f.stack.canRedo());
}

TEST_CASE("Structural writes require explicit history or framework cleanup", "[UndoStack][structural]")
{
    UndoFixture f;
    REQUIRE_THROWS_AS(f.mgr.addModel("bare", { }), ModelOperationBusy);
    REQUIRE_FALSE(f.mgr.modelById(0));
    REQUIRE_FALSE(f.stack.canUndo());
    Index model;
    {
        ModelScope initialization(f.mgr, &f.stack, { }, ModelScope::Kind::Cleanup);
        model = f.mgr.addModel("seed", { });
    }
    REQUIRE_FALSE(f.stack.canUndo());
    REQUIRE_THROWS_AS(f.mgr.removeModel(model), ModelOperationBusy);
    REQUIRE(f.mgr.modelById(model));
    {
        ModelScope command(f.mgr, &f.stack, "delete");
        f.mgr.removeModel(model);
    }
    REQUIRE(f.stack.undo());
    REQUIRE(f.mgr.modelById(model));
}

TEST_CASE("Structural undo and redo preserve identities over repeated cycles", "[UndoStack][structural]")
{
    bool component_operation = false;
    bool added = false;
    SECTION("add model") { added = true; }
    SECTION("remove model") { }
    SECTION("add component")
    {
        component_operation = true;
        added = true;
    }
    SECTION("remove component") { component_operation = true; }
    UndoFixture f;
    const auto [seed_model, seed_component] = f.addTriangle();
    f.stack.clear();
    Index model_id = seed_model;
    Index component_id = seed_component;
    std::vector<Index> point_gids;
    std::vector<GeomFaceId> face_gids;
    {
        ModelScope operation(f.mgr, &f.stack, "structure");
        if (added) {
            if (component_operation)
                component_id = addGeometryTriangle(f.mgr, model_id);
            else {
                auto identity = f.addTriangle("added");
                model_id = identity.first;
                component_id = identity.second;
            }
        }
        auto* data = f.mgr.findComponent(component_id);
        REQUIRE(data);
        point_gids = data->point_global_ids_;
        REQUIRE(point_gids.size() == 3);
        if (data->geometry)
            face_gids = data->geometry->index.face_local_to_global;
        // 同一个记录内含组件写，恢复必须保留结构与组件快照的先后顺序。
        writeVertex(f.mgr, component_id, 0, { 9, 0, 0 });
        if (!added) {
            if (component_operation)
                f.mgr.removeComponent(component_id);
            else
                f.mgr.removeModel(model_id);
        }
    }
    auto check_state = [&](bool present, std::array<double, 3> position) {
        auto* data = f.mgr.findComponent(component_id);
        REQUIRE((data != nullptr) == present);
        REQUIRE((f.mgr.modelById(model_id) != nullptr) == (component_operation || present));
        if (!present)
            return;
        REQUIRE(vertexAt(f.mgr, component_id, 0) == position);
        REQUIRE(data->point_global_ids_ == point_gids);
        REQUIRE(f.mgr.getComponentOperator(component_id)->modelId() == model_id);
        for (Index local = 0; local < 3; ++local)
            REQUIRE(f.mgr.pointIdMap().getLocal(point_gids[static_cast<std::size_t>(local)])
                == std::pair<Index, Index> { component_id, local });
        if (!face_gids.empty()) {
            REQUIRE(data->geometry);
            REQUIRE(data->geometry->index.face_local_to_global == face_gids);
            for (std::size_t local = 1; local < face_gids.size(); ++local)
                REQUIRE(f.mgr.geomRegistry().getFace(face_gids[local]));
        }
    };
    for (int cycle = 0; cycle < 3; ++cycle) {
        INFO(cycle);
        REQUIRE(f.stack.undo());
        check_state(!added, { 0, 0, 0 });
        REQUIRE_FALSE(f.stack.canUndo());
        REQUIRE(f.stack.canRedo());
        REQUIRE(f.stack.redo());
        check_state(added, { 9, 0, 0 });
        REQUIRE(f.stack.undoLabel() == "structure");
        REQUIRE_FALSE(f.stack.canRedo());
    }
}

TEST_CASE("Mixed structural sequences restore owners before components", "[UndoStack][structural]")
{
    bool added = false;
    SECTION("model then component are added") { added = true; }
    SECTION("component then model are removed") { }
    UndoFixture f;
    auto [model_id, component_id] = f.addTriangle();
    f.stack.clear();
    std::vector<Index> point_gids;
    {
        ModelScope operation(f.mgr, &f.stack, "mixed structure");
        if (added) {
            model_id = f.mgr.addModel("new owner", { });
            component_id = addGeometryTriangle(f.mgr, model_id);
        }
        point_gids = f.mgr.findComponent(component_id)->point_global_ids_;
        writeVertex(f.mgr, component_id, 0, { 6, 0, 0 });
        if (!added) {
            f.mgr.removeComponent(component_id);
            f.mgr.removeModel(model_id);
        }
    }
    auto check_presence = [&](bool present, std::array<double, 3> position) {
        REQUIRE((f.mgr.modelById(model_id) != nullptr) == present);
        REQUIRE((f.mgr.findComponent(component_id) != nullptr) == present);
        if (present) {
            REQUIRE(f.mgr.modelById(model_id)->componentIds() == std::vector<Index> { component_id });
            REQUIRE(vertexAt(f.mgr, component_id, 0) == position);
            REQUIRE(f.mgr.findComponent(component_id)->point_global_ids_ == point_gids);
        }
    };
    for (int cycle = 0; cycle < 3; ++cycle) {
        INFO(cycle);
        REQUIRE(f.stack.undo());
        check_presence(!added, { 0, 0, 0 });
        REQUIRE_FALSE(f.stack.canUndo());
        REQUIRE(f.stack.redo());
        check_presence(added, { 6, 0, 0 });
        REQUIRE_FALSE(f.stack.canRedo());
    }
}

TEST_CASE("Preview revert consumes structural captures before subsequent cancel", "[UndoStack][structural][preview]")
{
    UndoFixture f;
    const auto [model_id, component_id] = f.addTriangle();
    const auto point_gids = f.mgr.findComponent(component_id)->point_global_ids_;
    f.stack.clear();
    REQUIRE(f.stack.beginScope("preview"));
    const auto scope_id = f.stack.scopeId();
    writeVertex(f.mgr, component_id, 0, { 9, 0, 0 });
    f.mgr.removeModel(model_id);
    const auto [temporary_model, temporary_component] = f.addTriangle("temporary");
    writeVertex(f.mgr, temporary_component, 0, { 5, 0, 0 });
    f.stack.revertScope();
    REQUIRE(f.stack.scopeId() == scope_id);
    REQUIRE_FALSE(f.mgr.modelById(temporary_model));
    REQUIRE_FALSE(f.mgr.findComponent(temporary_component));
    REQUIRE(vertexAt(f.mgr, component_id, 0) == std::array<double, 3> { 0, 0, 0 });
    REQUIRE(f.mgr.findComponent(component_id)->point_global_ids_ == point_gids);
    writeVertex(f.mgr, component_id, 0, { 7, 0, 0 });
    f.mgr.removeComponent(component_id);
    const auto [next_model, next_component] = f.addTriangle("next temporary");
    f.stack.cancelScope();
    REQUIRE_FALSE(f.stack.scopeActive());
    REQUIRE_FALSE(f.mgr.modelById(next_model));
    REQUIRE_FALSE(f.mgr.findComponent(next_component));
    REQUIRE(f.mgr.modelById(model_id)->componentIds() == std::vector<Index> { component_id });
    REQUIRE(vertexAt(f.mgr, component_id, 0) == std::array<double, 3> { 0, 0, 0 });
    REQUIRE(f.mgr.findComponent(component_id)->point_global_ids_ == point_gids);
    REQUIRE_FALSE(f.stack.canUndo());
    REQUIRE_FALSE(f.stack.canRedo());
}

TEST_CASE("Closed sessions cannot absorb records when the same owner reenters", "[UndoStack][session]")
{
    UndoFixture f;
    const auto [mid, cid] = f.addTriangle();
    f.stack.clear();
    UndoStack::OwnerScope owner(&f.stack, "feature");
    auto write = [&](const char* label, double x) {
        ModelScope scope(f.mgr, &f.stack, label);
        writeVertex(f.mgr, cid, 0, { x, 0, 0 });
    };
    f.stack.beginSession("feature", "old session");
    write("old step", 1);
    SECTION("old undo segment behind a foreign record")
    {
        {
            ModelScope scope(f.mgr, &f.stack, "foreign", ModelScope::Kind::Command, "other");
            writeVertex(f.mgr, cid, 1, { 9, 0, 0 });
        }
        SECTION("empty final segment") { }
        SECTION("one final record") { write("final step", 2); }
        SECTION("multiple final records")
        {
            write("final step 1", 2);
            write("final step 2", 3);
        }
        f.stack.endSession("feature");
        while (f.stack.undoLabel() != "old step")
            REQUIRE(f.stack.undo());
    }
    SECTION("old record on redo stack")
    {
        REQUIRE(f.stack.undo());
        f.stack.endSession("feature");
        REQUIRE(f.stack.redo());
    }
    f.stack.beginSession("feature", "new session");
    write("new step", 4);
    f.stack.endSession("feature");
    REQUIRE(f.stack.undo());
    CHECK(vertexAt(f.mgr, cid, 0)[0] == 1);
    CHECK(f.stack.undoLabel() == "old step");
}

TEST_CASE("Opening and replacing previews notifies observable undo state", "[UndoStack][preview]")
{
    UndoFixture f;
    std::vector<std::string> labels;
    f.stack.setOnChanged([&] {
        if (f.stack.scopeActive()) {
            CHECK(f.stack.canUndo());
            CHECK_FALSE(f.stack.canRedo());
            labels.push_back(*f.stack.undoLabel());
        }
    });
    REQUIRE(f.stack.beginScope("first"));
    REQUIRE(labels == std::vector<std::string> { "first" });
    REQUIRE(f.stack.beginScope("second"));
    REQUIRE(labels == std::vector<std::string> { "first", "second" });
    f.stack.setOnChanged({ });
}
