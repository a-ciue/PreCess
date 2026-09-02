/**
 * @file TestUndoStack.cpp
 * @brief UndoStack 单元测试：边界自动记录、结构记录、staged 会话、执行路径完备性
 */
#include "ComponentData.h"
#include "ComponentOperator.h"
#include "MeshData.h"
#include "MeshIDMap.h"
#include "ModelLayer.h"
#include "ModelObserver.h"
#include "UndoStack.h"

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
    void notifyModelAdded(Index) override { }
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

    f.mgr.removeModel(model_id);
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

TEST_CASE("UndoStack staged session commit/cancel/revert", "[UndoStack][staged]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear();

    // beginStaged → 写 → commitStaged：恰一条记录，undo 回到 begin 前
    REQUIRE(f.stack.beginStaged("预览", cid));
    REQUIRE(f.stack.stagedActive());
    writeVertex(f.mgr, cid, 0, { 5.0, 5.0, 5.0 });
    f.stack.commitStaged();
    REQUIRE_FALSE(f.stack.stagedActive());
    REQUIRE(f.stack.canUndo());
    REQUIRE(f.stack.undoLabel() == "预览");
    f.stack.undo();
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 });
    REQUIRE_FALSE(f.stack.canUndo());

    // beginStaged → 写 → cancelStaged：无记录且状态回滚
    REQUIRE(f.stack.beginStaged("预览2", cid));
    writeVertex(f.mgr, cid, 0, { 7.0, 7.0, 7.0 });
    f.stack.cancelStaged();
    REQUIRE_FALSE(f.stack.stagedActive());
    REQUIRE_FALSE(f.stack.canUndo());
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 });

    // revertStaged：回滚但会话保持（可再写再 commit）
    REQUIRE(f.stack.beginStaged("预览3", cid));
    writeVertex(f.mgr, cid, 0, { 7.0, 7.0, 7.0 });
    f.stack.revertStaged();
    REQUIRE(f.stack.stagedActive());
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 });
    writeVertex(f.mgr, cid, 0, { 9.0, 9.0, 9.0 });
    f.stack.commitStaged();
    REQUIRE(f.stack.canUndo());
    f.stack.undo();
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 });

    // 重复 beginStaged 抛 std::runtime_error
    REQUIRE(f.stack.beginStaged("预览4", cid));
    REQUIRE_THROWS_AS(f.stack.beginStaged("预览5", cid), std::runtime_error);
    f.stack.cancelStaged();
}

TEST_CASE("UndoStack undo during staged session cancels it without touching global stack", "[UndoStack][staged]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear();

    // 先制造一条全局记录
    f.stack.beginOperation("前置编辑");
    writeVertex(f.mgr, cid, 1, { 4.0, 4.0, 4.0 });
    f.stack.commitOperation();

    REQUIRE(f.stack.beginStaged("预览", cid));
    writeVertex(f.mgr, cid, 0, { 5.0, 5.0, 5.0 });

    // staged 打开时 undo = cancelStaged：恢复 before₀ 并关闭会话，全局栈记录不动
    f.stack.undo();
    REQUIRE_FALSE(f.stack.stagedActive());
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 });
    REQUIRE(f.stack.canUndo());
    REQUIRE(f.stack.undoLabel() == "前置编辑");
}

TEST_CASE("UndoStack read-only operation boundary does not cancel an open staged session", "[UndoStack][staged]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear();

    REQUIRE(f.stack.beginStaged("预览", cid));
    writeVertex(f.mgr, cid, 0, { 5.0, 5.0, 5.0 });

    // 纯旁观回调（只读事件订阅同样走操作边界）：不得误杀进行中的预览
    f.stack.beginOperation("旁观回调");
    f.stack.commitOperation();
    REQUIRE(f.stack.stagedActive());
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 5.0, 5.0, 5.0 });
    REQUIRE(f.stack.undoLabel() == "预览"); // 空操作丢弃，无新记录

    // 会话仍可正常收尾
    f.stack.cancelStaged();
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 });
}

TEST_CASE("UndoStack real write inside a boundary implicitly cancels an open staged session", "[UndoStack][staged]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear();

    REQUIRE(f.stack.beginStaged("旧预览", cid));
    writeVertex(f.mgr, cid, 0, { 5.0, 5.0, 5.0 });

    // staged 打开时新操作发生真实写入（模拟切换算法）：旧预览隐式回滚，新操作正常成记录
    f.stack.beginOperation("新操作");
    REQUIRE(f.stack.stagedActive()); // 边界本身不杀 staged，写入点才杀
    writeVertex(f.mgr, cid, 1, { 6.0, 6.0, 6.0 });
    REQUIRE_FALSE(f.stack.stagedActive());
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 }); // before₀ 已恢复
    f.stack.commitOperation();
    REQUIRE(f.stack.canUndo());
    REQUIRE(f.stack.undoLabel() == "新操作");

    // undo 一步回到 before₀（before-image 是恢复后的状态，不含旧预览残留）
    f.stack.undo();
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 });
    REQUIRE(vertexAt(f.mgr, cid, 1) == std::array<double, 3> { 1.0, 0.0, 0.0 });

    // 旧功能后续 staged 调用发现会话已关：空转容忍不崩
    f.stack.redo();
    f.stack.commitStaged();
    f.stack.cancelStaged();
    f.stack.revertStaged();
    REQUIRE(f.stack.undoLabel() == "新操作");
}

TEST_CASE("UndoStack write after implicit cancel still notifies at boundary flush", "[UndoStack][staged]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear();

    REQUIRE(f.stack.beginStaged("旧预览", cid));
    writeVertex(f.mgr, cid, 0, { 5.0, 5.0, 5.0 });
    f.mgr.flushNotifications(); // 清掉预览写的待通知，只观察后续通知

    // 隐式 cancelStaged 内部 flush（恢复通知）不得吞掉本次写入的待通知
    const int notify_before = f.obs.component_changed_count;
    f.stack.beginOperation("新操作");
    writeVertex(f.mgr, cid, 1, { 6.0, 6.0, 6.0 });
    f.mgr.flushNotifications(); // 操作边界收尾 flush
    // 恢复 before₀ 一次 + 本次写入一次（去重前各自入过待通知集合）
    REQUIRE(f.obs.component_changed_count == notify_before + 2);
}

TEST_CASE("UndoStack structural operation implicitly cancels an open staged session", "[UndoStack][staged]")
{
    UndoFixture f;
    const auto [model_id, cid] = f.addTriangle();
    f.stack.clear();

    REQUIRE(f.stack.beginStaged("预览", cid));
    writeVertex(f.mgr, cid, 0, { 5.0, 5.0, 5.0 });

    // 边界外结构操作（如 QML 删除模型）：真实写入点，先隐式回滚预览再记录
    f.mgr.removeModel(model_id);
    REQUIRE_FALSE(f.stack.stagedActive());
    REQUIRE(f.stack.canUndo());
    REQUIRE(f.stack.undoLabel() == "删除模型");

    // undo 恢复的是 before₀（预览已回滚），不含预览残留
    f.stack.undo();
    REQUIRE(vertexAt(f.mgr, cid, 0) == std::array<double, 3> { 0.0, 0.0, 0.0 });
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
