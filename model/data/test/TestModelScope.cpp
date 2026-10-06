/**
 * @file TestModelScope.cpp
 * @brief ModelScope 单元测试：空操作零记录、异常路径收尾、固定入口与同步嵌套、特权越闸与关窗
 */
#include "ComponentData.h"
#include "ComponentOperator.h"
#include "MeshData.h"
#include "MeshIDMap.h"
#include "ModelLayer.h"
#include "ModelObserver.h"
#include "ModelScope.h"
#include "UndoStack.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <memory>
#include <source_location>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
//! @brief 计数观察者：notifyComponentChanged 累加（可选开关使其抛异常，模拟收尾 flush 失败）
struct CountingObserver : ModelObserver {
    int component_changed_count { 0 };
    bool throw_on_notify { false };

    void notifyModelChanged(Index) override { }
    void notifyModelAdded(Index) override { }
    void notifyModelRemoved(Index) override { }
    void notifyComponentRemoved(Index) override { }
    void notifyComponentChanged(Index) override
    {
        ++component_changed_count;
        if (throw_on_notify)
            throw std::runtime_error("observer failed");
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

//! @brief ModelLayer + UndoStack 挂接的测试夹具（与 TestUndoStack 同构）
struct ScopeFixture {
    CountingObserver obs;
    ModelLayer mgr { &obs };
    UndoStack stack { mgr };

    ScopeFixture() { mgr.setUndoRecorder(&stack); }

    //! @brief 入池一个三角形组件并清掉即时结构记录，返回 component_id
    Index addTriangle(const std::string& model_name = "test_model")
    {
        std::optional<ModelScope> scope;
        if (!stack.inOperation() && !stack.scopeActive())
            scope.emplace(mgr, &stack, "添加模型");
        ComponentDatas comps;
        comps.push_back(makeTriangleComponent("Comp_0"));
        const Index model_id = mgr.addModel(model_name, std::move(comps));
        scope.reset();
        stack.clear(); // 初始化记录不计入本用例
        return mgr.modelById(model_id)->componentIds()[0];
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
}

TEST_CASE("ModelScope discards an empty operation as zero record", "[ModelScope]")
{
    ScopeFixture f;
    const Index cid = f.addTriangle();
    REQUIRE_FALSE(f.stack.canUndo());

    {
        ModelScope scope(f.mgr, &f.stack, "空操作");
        // 不写模型
    }
    REQUIRE_FALSE(f.stack.canUndo());
    REQUIRE(f.obs.component_changed_count == 0);
}

TEST_CASE("Boundary-less component write rejects before data or adjacency changes", "[ModelScope][operation]")
{
    ScopeFixture f;
    const Index cid = f.addTriangle();
    auto* component = f.mgr.findComponent(cid);
    const auto positions = component->mesh->vertex_positions_;
    const auto gids = component->point_global_ids_;
    auto edge = component->mesh_adjacency.findEdgeByEndpoints(*component->mesh, 0, 1);
    REQUIRE(edge);
    const auto sid = component->mesh_adjacency.edgeStableId(*component->mesh, *edge);
    REQUIRE(sid);
    const auto location = std::source_location::current();
    std::string error;
    try {
        f.mgr.getComponentOperator(cid)->appendPoint({ 9, 9, 9 }, location);
    } catch (const ModelOperationBusy& e) {
        error = e.what();
    }
    REQUIRE(error.find(std::string(location.file_name()) + ":" + std::to_string(location.line())) != std::string::npos);
    REQUIRE(component->mesh->vertex_positions_ == positions);
    REQUIRE(component->point_global_ids_ == gids);
    // 原句柄仍有效：写闸不能在拒绝前失效或重建邻接缓存。
    REQUIRE(component->mesh_adjacency.edgeStableId(*component->mesh, *edge) == sid);
    f.mgr.flushNotifications();
    REQUIRE(f.obs.component_changed_count == 0);
    REQUIRE_FALSE(f.stack.canUndo());
    REQUIRE_FALSE(f.stack.canRedo());
}

TEST_CASE("ModelScope commits record and flushes notifications on normal exit", "[ModelScope]")
{
    ScopeFixture f;
    const Index cid = f.addTriangle();

    {
        ModelScope scope(f.mgr, &f.stack, "移动顶点");
        writeVertex(f.mgr, cid, 0, { 5.0, 5.0, 5.0 });
    }
    REQUIRE(f.stack.canUndo());
    REQUIRE(f.stack.undoLabel() == "移动顶点");
    REQUIRE(f.obs.component_changed_count == 1);
}

TEST_CASE("ModelScope commits record and flushes on exceptional exit (commit not rollback)", "[ModelScope]")
{
    ScopeFixture f;
    const Index cid = f.addTriangle();

    std::string caught;
    try {
        ModelScope scope(f.mgr, &f.stack, "部分写入");
        writeVertex(f.mgr, cid, 0, { 7.0, 7.0, 7.0 });
        throw std::runtime_error("body failed");
    } catch (const std::exception& e) {
        caught = e.what();
    }
    // 原异常原样传播；部分写入已成记录（可撤销）+ 通知已发
    REQUIRE(caught == "body failed");
    REQUIRE(f.stack.canUndo());
    REQUIRE(f.stack.undoLabel() == "部分写入");
    REQUIRE(f.obs.component_changed_count == 1);
}

TEST_CASE("ModelScope nested commands share one record and flush at their exits", "[ModelScope]")
{
    ScopeFixture f;
    const Index cid = f.addTriangle();

    {
        ModelScope scope(f.mgr, &f.stack, "外层");
        {
            ModelScope child(f.mgr, &f.stack, "内层");
            writeVertex(f.mgr, cid, 0, { 2.0, 2.0, 2.0 });
        }
        REQUIRE_FALSE(f.stack.canUndo());
        writeVertex(f.mgr, cid, 0, { 5.0, 5.0, 5.0 });
    }
    REQUIRE(f.stack.canUndo()); // 记录已成
    REQUIRE(f.stack.undoLabel() == "外层");
    REQUIRE(f.obs.component_changed_count == 2);
    REQUIRE(f.stack.undo());
    const std::array<double, 3> origin { 0, 0, 0 };
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_[0] == origin);
}

TEST_CASE("ModelScope with empty stack degrades to flush only", "[ModelScope]")
{
    ScopeFixture f;
    const Index cid = f.addTriangle();

    {
        ModelScope scope(f.mgr, nullptr, "无栈场景");
        // 无栈退化场景（深度 0、无层）的直接写无记账归属：写前拒绝，不进数据
        REQUIRE_THROWS(writeVertex(f.mgr, cid, 0, { 5.0, 5.0, 5.0 }));
    }
    REQUIRE_FALSE(f.stack.canUndo()); // 不成记录
    REQUIRE(f.obs.component_changed_count == 0); // 无写入 → flush 无通知可发
}

TEST_CASE("Model operation release is identity bound and does not unlock its successor", "[ModelScope][operation]")
{
    ScopeFixture f;
    auto first = f.mgr.beginWriteOperation(true);
    REQUIRE(first);
    REQUIRE(f.mgr.writesFrozen());
    REQUIRE_FALSE(f.mgr.beginWriteOperation());
    auto transferred = std::move(first);
    REQUIRE_FALSE(first);
    REQUIRE(transferred->active());
    first.reset();
    REQUIRE(f.mgr.writesPending());
    REQUIRE(f.mgr.writesFrozen());
    transferred->release();
    auto second = f.mgr.beginWriteOperation();
    REQUIRE(second);
    transferred->release();
    transferred.reset(); // 已释放的旧凭证析构不能释放后继操作。
    REQUIRE(f.mgr.writesPending());
    REQUIRE_FALSE(f.mgr.writesFrozen());
    second->release();
    REQUIRE_FALSE(f.mgr.writesPending());
}

TEST_CASE("Operation authority survives model movement and model destruction", "[ModelScope][operation]")
{
    std::unique_ptr<ModelLayer::WriteOperation> operation;
    {
        ModelLayer source;
        operation = source.beginWriteOperation(true);
        REQUIRE(operation);
        ModelLayer target(std::move(source));
        REQUIRE(source.writesPending());
        REQUIRE(target.writesFrozen());
        operation.reset();
        REQUIRE_FALSE(source.writesPending());
        REQUIRE_FALSE(target.writesPending());
        operation = target.beginWriteOperation();
    }
    // 凭证仍持控制块；模型对象先析构时，查询和释放不访问模型地址。
    REQUIRE(operation->active());
    REQUIRE_NOTHROW(operation.reset());
}

TEST_CASE("Model operation accepts its GUI write segment and rejects missing or foreign authority", "[ModelScope][operation]")
{
    ScopeFixture f;
    const Index cid = f.addTriangle();
    auto operation = f.mgr.beginWriteOperation(true);
    REQUIRE_THROWS_AS(writeVertex(f.mgr, cid, 0, { 1, 1, 1 }), ModelOperationBusy);
    REQUIRE_THROWS_AS(ModelLayer::WritePrivilege(f.mgr), ModelOperationBusy);
    ModelLayer other;
    REQUIRE_THROWS_AS(ModelLayer::WritePrivilege(other, operation.get()), std::runtime_error);
    {
        ModelLayer::WritePrivilege privilege(f.mgr, operation.get());
        ModelScope scope(f.mgr, &f.stack, "提交写");
        writeVertex(f.mgr, cid, 0, { 5, 5, 5 });
    }
    REQUIRE(f.stack.canUndo());
    REQUIRE(f.mgr.writesPending());
    operation->release();
    f.stack.undo();
    const std::array<double, 3> origin { 0, 0, 0 };
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_[0] == origin);
}

TEST_CASE("GUI privilege never grants worker access to the real model", "[ModelScope][operation]")
{
    ScopeFixture f;
    const Index cid = f.addTriangle();
    auto operation = f.mgr.beginWriteOperation();
    bool rejected = false;
    {
        ModelLayer::WritePrivilege privilege(f.mgr, operation.get());
        ModelScope scope(f.mgr, &f.stack, "线程闸");
        auto op = f.mgr.getComponentOperator(cid);
        std::thread worker([&] {
            try {
                op->appendPoint({ 9, 9, 9 });
            } catch (const std::runtime_error&) {
                rejected = true;
            }
        });
        worker.join();
    }
    REQUIRE(rejected);
    REQUIRE_FALSE(f.stack.canUndo());
    REQUIRE(f.mgr.findComponent(cid)->mesh->vertex_positions_.size() == 3);
}

TEST_CASE("ModelScope swallows teardown exceptions without replacing the original", "[ModelScope]")
{
    ScopeFixture f;
    const Index cid = f.addTriangle();
    f.obs.throw_on_notify = true; // flush 抛出 → 析构内吞掉记日志

    std::string caught;
    try {
        ModelScope scope(f.mgr, &f.stack, "收尾异常");
        writeVertex(f.mgr, cid, 0, { 5.0, 5.0, 5.0 });
        throw std::runtime_error("body failed");
    } catch (const std::exception& e) {
        caught = e.what(); // 必须是原异常，不是 observer 的
    }
    REQUIRE(caught == "body failed");
    REQUIRE(f.stack.canUndo()); // 收尾顺序：commit 先于 flush，记录不受 flush 失败影响
}
TEST_CASE("Notification observer cannot inherit completed commit write authority", "[ModelScope][operation]")
{
    struct WritingObserver final : CountingObserver {
        ModelLayer* layer { };
        bool rejected { false };
        void notifyComponentChanged(Index id) override
        {
            if (!layer)
                return;
            try {
                layer->getComponentOperator(id)->appendPoint({ 9, 9, 9 });
            } catch (const ModelOperationBusy&) {
                rejected = true;
            }
        }
    } observer;
    ModelLayer model(&observer);
    ComponentDatas components;
    components.push_back(makeTriangleComponent("target"));
    const Index mid = model.addModel("model", std::move(components));
    const Index cid = model.modelById(mid)->componentIds()[0];
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    observer.layer = &model;
    auto operation = model.beginWriteOperation();
    {
        ModelLayer::WritePrivilege privilege(model, operation.get());
        ModelScope scope(model, &undo, "commit");
        model.getComponentOperator(cid)->appendPoint({ 4, 0, 0 });
    }
    REQUIRE(observer.rejected);
    REQUIRE(model.findComponent(cid)->mesh->vertex_positions_.size() == 4);
    observer.layer = nullptr;
    operation->release();
    REQUIRE(undo.undo());
    REQUIRE(model.findComponent(cid)->mesh->vertex_positions_.size() == 3);
    REQUIRE(undo.redo());
    REQUIRE(model.findComponent(cid)->mesh->vertex_positions_.size() == 4);
}

TEST_CASE("ModelScope rejects a formal boundary while another operation owns the model", "[ModelScope][operation]")
{
    ScopeFixture fixture;
    const Index component = fixture.addTriangle();
    auto operation = fixture.mgr.beginWriteOperation(false);
    REQUIRE(operation);
    bool entered = false;
    REQUIRE_THROWS_AS(([&] {
        ModelScope scope(fixture.mgr, &fixture.stack, "插队");
        entered = true;
    })(),
        ModelOperationBusy);
    REQUIRE_FALSE(entered);
    REQUIRE_FALSE(fixture.stack.inOperation());
    REQUIRE_FALSE(fixture.stack.canUndo());
    REQUIRE(fixture.mgr.writesPending());
    operation->release();
    {
        ModelScope scope(fixture.mgr, &fixture.stack, "正常操作");
        writeVertex(fixture.mgr, component, 0, { 2, 2, 2 });
    }
    REQUIRE(fixture.stack.undoLabel() == "正常操作");
}
