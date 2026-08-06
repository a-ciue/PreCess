#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "MeshData.h"
#include "MeshIDMap.h"
#include "ComponentData.h"
#include "ModelLayer.h"
#include "ModelObserver.h"
#include "ModelSnapshot.h"
#include "ComponentOperator.h"
#include "MakeMeshData.h"

namespace {
struct CountingObserver : ModelObserver {
    int component_changed_count { 0 };
    int model_added_count { 0 };
    Index last_component_changed { -1 };
    Index last_model_added { -1 };

    void notifyModelChanged(Index) override { }
    void notifyModelAdded(Index model_id) override
    {
        ++model_added_count;
        last_model_added = model_id;
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
}

TEST_CASE("MeshData::clone produces an independent deep copy", "[MeshData][snapshot]")
{
    MeshData mesh = MakeMeshData();
    mesh.vertex_count_ = (Index)mesh.vertex_positions_.size();
    mesh.vertex_attributes_["v_scalar_1"] = { 1.0, 2.0, 3.0 };
    mesh.face_attributes_["f_scalar_1"] = { 4.0 };

    auto clone = mesh.clone();
    REQUIRE(clone);

    // 内容逐字段相等（patches_/blocks_ 为待删设施，不进快照）
    REQUIRE(clone->vertex_positions_ == mesh.vertex_positions_);
    REQUIRE(clone->vertex_count_ == mesh.vertex_count_);
    REQUIRE(clone->face_vertices_ == mesh.face_vertices_);
    REQUIRE(clone->face_vertices_offset_ == mesh.face_vertices_offset_);
    REQUIRE(clone->edge_vertices_ == mesh.edge_vertices_);
    REQUIRE(clone->solid_types_ == mesh.solid_types_);
    REQUIRE(clone->solid_vertices_ == mesh.solid_vertices_);
    REQUIRE(clone->solid_vertices_offset_ == mesh.solid_vertices_offset_);
    REQUIRE(clone->solid_faces_vertices_ == mesh.solid_faces_vertices_);
    REQUIRE(clone->solid_faces_vertices_offset_ == mesh.solid_faces_vertices_offset_);
    REQUIRE(clone->solid_faces_ == mesh.solid_faces_);
    REQUIRE(clone->solid_faces_offset_ == mesh.solid_faces_offset_);
    REQUIRE(clone->vertex_attributes_ == mesh.vertex_attributes_);
    REQUIRE(clone->face_attributes_ == mesh.face_attributes_);
    REQUIRE(clone->patches_.empty());
    REQUIRE(clone->blocks_.empty());

    // 改原对象（删面、改坐标、改属性），克隆不受影响
    const auto original_positions = clone->vertex_positions_;
    const auto original_faces = clone->face_vertices_;
    mesh.vertex_positions_[0] = { 9.0, 9.0, 9.0 };
    mesh.face_vertices_.clear();
    mesh.face_vertices_offset_ = { 0 };
    mesh.vertex_attributes_["v_scalar_1"][0] = 42.0;

    REQUIRE(clone->vertex_positions_ == original_positions);
    REQUIRE(clone->face_vertices_ == original_faces);
    REQUIRE(clone->vertex_attributes_.at("v_scalar_1")[0] == 1.0);
}

TEST_CASE("ComponentData::clone/restoreFrom round-trips data and keeps id", "[ComponentData][snapshot]")
{
    auto comp = makeTriangleComponent("Comp_0");
    comp->id = 7;
    comp->material_id = 3;
    comp->source_xde_leaf_id = 5;
    comp->point_global_ids_ = { 10, 11, 12 };
    comp->ensureMapping().geometry_edge_to_mesh_point_ids[100] = { 0, 1 };

    // 建立邻接持久身份层（触发稳定边 id 分配）
    MeshData& md = *comp->mesh;
    auto edge = comp->mesh_adjacency.findEdgeByEndpoints(md, 0, 1);
    REQUIRE(edge.has_value());
    const auto sid_before = comp->mesh_adjacency.edgeStableId(md, *edge);
    REQUIRE(sid_before.has_value());

    auto snapshot = comp->clone();
    REQUIRE(snapshot->id == 7);
    REQUIRE(snapshot->name == "Comp_0");
    REQUIRE(snapshot->point_global_ids_ == comp->point_global_ids_);
    REQUIRE(snapshot->material_id == 3);
    REQUIRE(snapshot->source_xde_leaf_id == 5);
    REQUIRE(snapshot->mapping);
    REQUIRE(snapshot->mapping->geometry_edge_to_mesh_point_ids == comp->mapping->geometry_edge_to_mesh_point_ids);

    // 克隆体的邻接懒表不带入，但持久身份层随拷贝延续：同一端点对给出同一稳定边 id
    auto cloned_edge = snapshot->mesh_adjacency.findEdgeByEndpoints(*snapshot->mesh, 0, 1);
    REQUIRE(cloned_edge.has_value());
    REQUIRE(snapshot->mesh_adjacency.edgeStableId(*snapshot->mesh, *cloned_edge) == sid_before);

    // 改原组件（删面），再 restoreFrom 还原；id 不随快照覆盖
    md.face_vertices_.clear();
    md.face_vertices_offset_ = { 0 };
    comp->mesh_adjacency.invalidate();
    snapshot->id = 99; // restoreFrom 不得覆盖本组件 id

    comp->restoreFrom(*snapshot);
    REQUIRE(comp->id == 7);
    REQUIRE(comp->mesh->face_vertices_ == std::vector<Index> { 0, 1, 2 });
    REQUIRE(comp->mesh->face_vertices_offset_ == std::vector<Index> { 0, 3 });
    REQUIRE(comp->point_global_ids_ == std::vector<Index> { 10, 11, 12 });
    REQUIRE(comp->material_id == 3);

    // 懒表不带入但下次查询自动重建，结果与快照前一致
    auto restored_edge = comp->mesh_adjacency.findEdgeByEndpoints(*comp->mesh, 0, 1);
    REQUIRE(restored_edge.has_value());
    REQUIRE(comp->mesh_adjacency.edgeStableId(*comp->mesh, *restored_edge) == sid_before);
}

TEST_CASE("restoreSnapshot restores data and reclaims gids", "[ComponentOperator][snapshot]")
{
    CountingObserver obs;
    ModelLayer mgr(&obs);

    ComponentDatas comps;
    comps.push_back(makeTriangleComponent("Comp_0"));
    const Index model_id = mgr.addModel("snapshot_test", std::move(comps));
    const Index component_id = mgr.modelById(model_id)->componentIds()[0];

    ComponentData* comp = mgr.findComponent(component_id);
    REQUIRE(comp);

    auto op = mgr.getComponentOperator(component_id);
    REQUIRE(op.has_value());
    auto snapshot = op->takeSnapshot();
    const std::vector<Index> gids_before = comp->point_global_ids_;
    REQUIRE(gids_before.size() == 3);

    // 就地编辑：删面 + 加点（经可写入口，Topology 标脏即时失效邻接懒表）
    MeshData& md = op->editableMesh();
    md.face_vertices_.clear();
    md.face_vertices_offset_ = { 0 };
    md.vertex_positions_.push_back({ 2.0, 0.0, 0.0 });
    md.vertex_count_ = (Index)md.vertex_positions_.size();
    REQUIRE(comp->mesh->vertex_positions_.size() == 4);

    op->restoreSnapshot(*snapshot);

    // 数据还原
    REQUIRE(comp->mesh->face_vertices_ == std::vector<Index> { 0, 1, 2 });
    REQUIRE(comp->mesh->vertex_positions_.size() == 3);
    REQUIRE(comp->point_global_ids_ == gids_before);

    // 点 gid 原值 reclaim 成功：gid -> (component_id, local)
    for (Index local = 0; local < 3; ++local) {
        auto [cid, lid] = mgr.pointIdMap().getLocal(gids_before[(size_t)local]);
        REQUIRE(cid == component_id);
        REQUIRE(lid == local);
    }

    // 边 gid 同样原值 reclaim：gid -> (component_id, sid)
    for (Index sid = 0; sid < 3; ++sid) {
        const auto gid = comp->mesh_adjacency.edgeGlobalId(*comp->mesh, sid);
        REQUIRE(gid.has_value());
        auto [cid, lid] = mgr.edgeIdMap().getLocal(*gid);
        REQUIRE(cid == component_id);
        REQUIRE(lid == sid);
    }
}

TEST_CASE("restoreSnapshot after removeMesh reclaims gids from free-list", "[ComponentOperator][snapshot]")
{
    CountingObserver obs;
    ModelLayer mgr(&obs);

    ComponentDatas comps;
    comps.push_back(makeTriangleComponent("Comp_0"));
    const Index model_id = mgr.addModel("snapshot_after_remove", std::move(comps));
    const Index component_id = mgr.modelById(model_id)->componentIds()[0];
    ComponentData* comp = mgr.findComponent(component_id);
    REQUIRE(comp);

    auto op = mgr.getComponentOperator(component_id);
    REQUIRE(op.has_value());
    auto snapshot = op->takeSnapshot();
    const std::vector<Index> gids_before = comp->point_global_ids_;

    // removeMesh 释放全部 gid 进 free-list（标脏但通知延迟到操作边界 flush）
    op->removeMesh();
    REQUIRE(!comp->mesh);
    REQUIRE(mgr.pointIdMap().freeSize() >= 3);
    const int changed_after_remove = obs.component_changed_count;

    op->restoreSnapshot(*snapshot);

    // mesh 还原、gid 原值 reclaim
    REQUIRE(comp->mesh);
    REQUIRE(comp->mesh->face_vertices_ == std::vector<Index> { 0, 1, 2 });
    REQUIRE(comp->point_global_ids_ == gids_before);
    for (Index local = 0; local < 3; ++local) {
        auto [cid, lid] = mgr.pointIdMap().getLocal(gids_before[(size_t)local]);
        REQUIRE(cid == component_id);
        REQUIRE(lid == local);
    }

    // 写必脏 + 操作边界 flush：removeMesh/restoreSnapshot 的标脏经 flush 去重后通知一次
    mgr.flushNotifications();
    REQUIRE(obs.component_changed_count == changed_after_remove + 1);
    REQUIRE(obs.last_component_changed == component_id);
}

TEST_CASE("MeshIDMap::reclaim validates target gid", "[MeshIDMap][snapshot]")
{
    MeshIDMap map;
    const auto gid0 = map.insert(1, 0);
    const auto gid1 = map.insert(1, 1);

    // 占用中的 gid 不可 reclaim；越界 gid 不可 reclaim
    REQUIRE_THROWS_AS(map.reclaim(gid0, 2, 0), std::runtime_error);
    REQUIRE_THROWS_AS(map.reclaim(100, 2, 0), std::runtime_error);
    REQUIRE_THROWS_AS(map.reclaim(-1, 2, 0), std::runtime_error);

    // 释放后可按原值 reclaim
    REQUIRE(map.remove(gid1));
    REQUIRE_NOTHROW(map.reclaim(gid1, 3, 5));
    auto [cid, lid] = map.getLocal(gid1);
    REQUIRE(cid == 3);
    REQUIRE(lid == 5);

    // reclaim 后 gid 已占用，再次 reclaim 抛异常
    REQUIRE_THROWS_AS(map.reclaim(gid1, 4, 6), std::runtime_error);
}

TEST_CASE("takeModelSnapshot/restoreModel round-trips model structure", "[ModelLayer][snapshot]")
{
    CountingObserver obs;
    ModelLayer mgr(&obs);

    ComponentDatas comps;
    comps.push_back(makeTriangleComponent("Comp_0"));
    comps.push_back(makeTriangleComponent("Comp_1"));
    const Index model_id = mgr.addModel("model_snapshot", std::move(comps));
    const auto comp_ids = mgr.modelById(model_id)->componentIds();
    REQUIRE(comp_ids.size() == 2);

    // 记录各组件点 gid
    std::vector<std::vector<Index>> gids_before;
    for (Index cid : comp_ids)
        gids_before.push_back(mgr.findComponent(cid)->point_global_ids_);

    auto snapshot = mgr.takeModelSnapshot(model_id);
    REQUIRE(snapshot->model_id == model_id);
    REQUIRE(snapshot->name == "model_snapshot");
    REQUIRE(snapshot->components.size() == 2);

    mgr.removeModel(model_id);
    REQUIRE(mgr.modelById(model_id) == nullptr);

    const int added_before = obs.model_added_count;
    const Index restored_id = mgr.restoreModel(*snapshot);
    REQUIRE(restored_id == model_id);
    REQUIRE(obs.model_added_count == added_before + 1);
    REQUIRE(obs.last_model_added == model_id);

    // 原 component_id 复原、组件数据一致、点 gid 原值 reclaim
    REQUIRE(mgr.modelById(model_id)->componentIds() == comp_ids);
    for (size_t i = 0; i < comp_ids.size(); ++i) {
        ComponentData* c = mgr.findComponent(comp_ids[i]);
        REQUIRE(c);
        REQUIRE(c->name == (i == 0 ? "Comp_0" : "Comp_1"));
        REQUIRE(c->mesh);
        REQUIRE(c->mesh->face_vertices_ == std::vector<Index> { 0, 1, 2 });
        REQUIRE(c->point_global_ids_ == gids_before[i]);
        for (Index local = 0; local < 3; ++local) {
            auto [cid2, lid] = mgr.pointIdMap().getLocal(gids_before[i][(size_t)local]);
            REQUIRE(cid2 == comp_ids[i]);
            REQUIRE(lid == local);
        }
    }

    // 占用冲突：model_id 仍在时 restoreModel 抛异常
    REQUIRE_THROWS_AS(mgr.restoreModel(*snapshot), std::runtime_error);
}

TEST_CASE("restoreComponent re-inserts component with original id", "[ModelLayer][snapshot]")
{
    CountingObserver obs;
    ModelLayer mgr(&obs);

    ComponentDatas comps;
    comps.push_back(makeTriangleComponent("Comp_0"));
    comps.push_back(makeTriangleComponent("Comp_1"));
    const Index model_id = mgr.addModel("component_restore", std::move(comps));
    const auto comp_ids = mgr.modelById(model_id)->componentIds();
    REQUIRE(comp_ids.size() == 2);
    const Index removed_id = comp_ids[1];

    // 组件级快照（clone，id 为原值）
    auto snapshot = mgr.findComponent(removed_id)->clone();
    const std::vector<Index> gids_before = mgr.findComponent(removed_id)->point_global_ids_;

    mgr.removeComponent(removed_id);
    REQUIRE(mgr.findComponent(removed_id) == nullptr);
    REQUIRE(mgr.modelById(model_id)->componentIds() == std::vector<Index> { comp_ids[0] });

    mgr.restoreComponent(model_id, std::move(snapshot));

    // 原 id 插回、component_to_model_ 正确、数据一致、gid 原值 reclaim
    ComponentData* c = mgr.findComponent(removed_id);
    REQUIRE(c);
    REQUIRE(c->name == "Comp_1");
    REQUIRE(mgr.modelById(model_id)->componentIds() == comp_ids);
    auto op = mgr.getComponentOperator(removed_id);
    REQUIRE(op.has_value());
    REQUIRE(op->modelId() == model_id);
    REQUIRE(c->point_global_ids_ == gids_before);
    for (Index local = 0; local < 3; ++local) {
        auto [cid2, lid] = mgr.pointIdMap().getLocal(gids_before[(size_t)local]);
        REQUIRE(cid2 == removed_id);
        REQUIRE(lid == local);
    }

    // 占用冲突：组件 id 已占用时 restoreComponent 抛异常
    REQUIRE_THROWS_AS(mgr.restoreComponent(model_id, c->clone()), std::runtime_error);
}
