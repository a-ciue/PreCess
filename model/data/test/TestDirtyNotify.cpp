#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "MeshData.h"
#include "MeshIDMap.h"
#include "ComponentData.h"
#include "ModelLayer.h"
#include "ModelObserver.h"
#include "ComponentOperator.h"

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

//! @brief 构造一个简单三角形面片组件（3 点 1 面，3 条面边）
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

//! @brief 组件入池，返回 (model_id, component_id)
std::pair<Index, Index> addTriangleModel(ModelLayer& mgr, const std::string& model_name)
{
    ComponentDatas comps;
    comps.push_back(makeTriangleComponent("Comp_0"));
    const Index model_id = mgr.addModel(model_name, std::move(comps));
    return { model_id, mgr.modelById(model_id)->componentIds()[0] };
}
}

TEST_CASE("editableMesh marks dirty; flush notifies once and clears", "[ComponentOperator][dirty]")
{
    CountingObserver obs;
    ModelLayer mgr(&obs);
    const auto [model_id, component_id] = addTriangleModel(mgr, "dirty_flush");
    const int count_after_add = obs.component_changed_count;

    auto op = mgr.getComponentOperator(component_id);
    REQUIRE(op.has_value());

    // 空 flush 无操作
    mgr.flushNotifications();
    REQUIRE(obs.component_changed_count == count_after_add);

    // 获取可写入口即标脏，但通知不即时发出
    MeshData& md = op->editableMesh();
    REQUIRE(md.vertex_positions_.size() == 3);
    REQUIRE(obs.component_changed_count == count_after_add);

    // flush 发一次 notifyComponentChanged 且集合清空
    mgr.flushNotifications();
    REQUIRE(obs.component_changed_count == count_after_add + 1);
    REQUIRE(obs.last_component_changed == component_id);
    mgr.flushNotifications();
    REQUIRE(obs.component_changed_count == count_after_add + 1);
}

TEST_CASE("multiple dirty marks on one component flush a single notification", "[ComponentOperator][dirty]")
{
    CountingObserver obs;
    ModelLayer mgr(&obs);
    const auto [model_id, component_id] = addTriangleModel(mgr, "dirty_dedup");
    const int count_after_add = obs.component_changed_count;

    auto op = mgr.getComponentOperator(component_id);
    REQUIRE(op.has_value());

    // 同一操作内多次标脏同一组件（可写入口 + 语义化方法）
    op->editableMesh();
    op->editableMesh(MeshEditKind::NonTopology);
    op->appendPoint({ 2.0, 0.0, 0.0 });

    // 去重：flush 只通知一次
    mgr.flushNotifications();
    REQUIRE(obs.component_changed_count == count_after_add + 1);
    REQUIRE(obs.last_component_changed == component_id);
}

TEST_CASE("NonTopology keeps adjacency handle valid; Topology invalidates", "[ComponentOperator][dirty]")
{
    CountingObserver obs;
    ModelLayer mgr(&obs);
    const auto [model_id, component_id] = addTriangleModel(mgr, "dirty_adjacency");

    ComponentData* comp = mgr.findComponent(component_id);
    REQUIRE(comp);
    MeshData& md = *comp->mesh;

    auto op = mgr.getComponentOperator(component_id);
    REQUIRE(op.has_value());

    // 签发当轮边表句柄与稳定边 id
    auto edge = comp->mesh_adjacency.findEdgeByEndpoints(md, 0, 1);
    REQUIRE(edge.has_value());
    const auto sid = comp->mesh_adjacency.edgeStableId(md, *edge);
    REQUIRE(sid.has_value());

    // NonTopology 标脏：邻接懒表不失效，句柄仍有效
    op->editableMesh(MeshEditKind::NonTopology);
    REQUIRE(comp->mesh_adjacency.edgeStableId(md, *edge) == sid);

    // Topology 标脏：邻接懒表立即失效，旧句柄不再有效（查询即时正确）
    op->editableMesh(MeshEditKind::Topology);
    REQUIRE_FALSE(comp->mesh_adjacency.edgeStableId(md, *edge).has_value());

    // 重新签发句柄：稳定边 id 跨拓扑编辑保持
    edge = comp->mesh_adjacency.findEdgeByEndpoints(md, 0, 1);
    REQUIRE(edge.has_value());
    REQUIRE(comp->mesh_adjacency.edgeStableId(md, *edge) == sid);
}

TEST_CASE("appendPoint performs the atomic four-step and marks dirty", "[ComponentOperator][dirty]")
{
    CountingObserver obs;
    ModelLayer mgr(&obs);
    const auto [model_id, component_id] = addTriangleModel(mgr, "append_point");

    ComponentData* comp = mgr.findComponent(component_id);
    REQUIRE(comp);

    auto op = mgr.getComponentOperator(component_id);
    REQUIRE(op.has_value());

    const Index local_id = op->appendPoint({ 2.0, 0.0, 0.0 });
    REQUIRE(local_id == 3);

    // 四连原子性：坐标、vertex_count_、gid 分配、gid 伴生表追加
    REQUIRE(comp->mesh->vertex_positions_.size() == 4);
    REQUIRE(comp->mesh->vertex_count_ == 4);
    REQUIRE(comp->point_global_ids_.size() == 4);
    const Index gid = comp->point_global_ids_[3];
    auto [cid, lid] = mgr.pointIdMap().getLocal(gid);
    REQUIRE(cid == component_id);
    REQUIRE(lid == local_id);
}

TEST_CASE("appendFace validates input and backfills empty offset with {0}", "[ComponentOperator][dirty]")
{
    CountingObserver obs;
    ModelLayer mgr(&obs);
    const auto [model_id, component_id] = addTriangleModel(mgr, "append_face");

    ComponentData* comp = mgr.findComponent(component_id);
    REQUIRE(comp);

    auto op = mgr.getComponentOperator(component_id);
    REQUIRE(op.has_value());

    // 常规追加：返回新面序号，offset 单调序列完整
    const Index face_id = op->appendFace({ 0, 1, 2 });
    REQUIRE(face_id == 1);
    REQUIRE(comp->mesh->face_vertices_.size() == 6);
    REQUIRE(comp->mesh->face_vertices_offset_ == std::vector<Index> { 0, 3, 6 });

    // 点数为 0 或局部 id 越界抛 std::invalid_argument
    REQUIRE_THROWS_AS(op->appendFace({}), std::invalid_argument);
    REQUIRE_THROWS_AS(op->appendFace({ 0, 1, 99 }), std::invalid_argument);
    REQUIRE_THROWS_AS(op->appendFace({ 0, 1, -1 }), std::invalid_argument);

    // 空 face_vertices_offset_ 先补 {0}
    MeshData& md = op->editableMesh();
    md.face_vertices_.clear();
    md.face_vertices_offset_.clear();
    const Index first_face_id = op->appendFace({ 0, 1, 2 });
    REQUIRE(first_face_id == 0);
    REQUIRE(comp->mesh->face_vertices_offset_ == std::vector<Index> { 0, 3 });
}

TEST_CASE("replaceMesh releases old gids, ensures new gids and marks dirty", "[ComponentOperator][dirty]")
{
    CountingObserver obs;
    ModelLayer mgr(&obs);
    const auto [model_id, component_id] = addTriangleModel(mgr, "replace_mesh");

    ComponentData* comp = mgr.findComponent(component_id);
    REQUIRE(comp);
    const std::vector<Index> old_gids = comp->point_global_ids_;
    REQUIRE(old_gids.size() == 3);
    const size_t free_before = mgr.pointIdMap().freeSize();
    const int count_before = obs.component_changed_count;

    auto op = mgr.getComponentOperator(component_id);
    REQUIRE(op.has_value());

    // 新网格：2 点 1 边单元（比旧网格少 1 点，使旧 gid 释放可经 free-list 观测）
    auto new_mesh = std::make_unique<MeshData>();
    new_mesh->init();
    new_mesh->vertex_positions_ = { { 0, 0, 0 }, { 1, 0, 0 } };
    new_mesh->edge_vertices_ = { 0, 1 };
    op->replaceMesh(std::move(new_mesh));

    // 旧点 gid 释放进 free-list：释放 3 个，ensure 新点复用 2 个，净空闲 +1
    REQUIRE(mgr.pointIdMap().freeSize() == free_before + 1);

    // 新点 gid ensure 补缺：gid -> (component_id, local)
    REQUIRE(comp->mesh);
    REQUIRE(comp->point_global_ids_.size() == 2);
    for (Index local = 0; local < 2; ++local) {
        auto [cid, lid] = mgr.pointIdMap().getLocal(comp->point_global_ids_[(size_t)local]);
        REQUIRE(cid == component_id);
        REQUIRE(lid == local);
    }

    // 新边 gid ensure：gid -> (component_id, sid)
    MeshData& md = *comp->mesh;
    auto edge = comp->mesh_adjacency.findEdgeByEndpoints(md, 0, 1);
    REQUIRE(edge.has_value());
    const auto sid = comp->mesh_adjacency.edgeStableId(md, *edge);
    REQUIRE(sid.has_value());
    const auto edge_gid = comp->mesh_adjacency.edgeGlobalId(md, *sid);
    REQUIRE(edge_gid.has_value());
    auto [cid, lid] = mgr.edgeIdMap().getLocal(*edge_gid);
    REQUIRE(cid == component_id);
    REQUIRE(lid == *sid);

    // 标脏待 flush：通知不即时发出，flush 后发一次
    REQUIRE(obs.component_changed_count == count_before);
    mgr.flushNotifications();
    REQUIRE(obs.component_changed_count == count_before + 1);
    REQUIRE(obs.last_component_changed == component_id);
}

TEST_CASE("editableMesh on a mesh-less component throws", "[ComponentOperator][dirty]")
{
    CountingObserver obs;
    ModelLayer mgr(&obs);

    auto c = std::make_unique<ComponentData>();
    c->name = "NoMesh";
    ComponentDatas comps;
    comps.push_back(std::move(c));
    const Index model_id = mgr.addModel("no_mesh", std::move(comps));
    const Index component_id = mgr.modelById(model_id)->componentIds()[0];

    auto op = mgr.getComponentOperator(component_id);
    REQUIRE(op.has_value());
    REQUIRE_THROWS_AS(op->editableMesh(), std::runtime_error);
    REQUIRE_THROWS_AS(op->editableMesh(MeshEditKind::NonTopology), std::runtime_error);
    REQUIRE_THROWS_AS(op->appendPoint({ 0.0, 0.0, 0.0 }), std::runtime_error);
    REQUIRE_THROWS_AS(op->appendFace({ 0, 1, 2 }), std::runtime_error);
}
