#include <catch2/catch_test_macros.hpp>
#include "ModelLayer.h"
#include "ComponentData.h"
#include "MeshData.h"
#include "MeshIDMap.h"
#include "ModelObserver.h"
#include <memory>

struct DummyObserver : ModelObserver {
    void notifyModelChanged(Index) override {}
    void notifyModelAdded(Index) override {}
    void notifyModelRemoved(Index) override {}
    void notifyComponentRemoved(Index) override {}
    void notifyComponentChanged(Index) override {}
    void notifyModelNameChanged(Index, const std::string&) override {}
    void notifyGeometryLoadFailed(const std::string&) override {}
};

TEST_CASE("Mesh self-contained points + point gid allocation on addModel", "[PointGlobalIds]")
{
    DummyObserver obs;
    ModelLayer mgr(&obs);

    auto mesh = std::make_unique<MeshData>();
    mesh->init();

    mesh->vertex_positions_ = {
        {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0}
    };

    // 一个三角面 (0,1,2)
    mesh->face_vertices_ = {0,1,2};
    mesh->face_vertices_offset_ = {0,3};

    // 一条真实线单元边 (0,1)
    mesh->edge_vertices_ = {0,1};

    auto c = std::make_unique<ComponentData>();
    c->id = -1;
    c->name = "Comp_0";
    c->mesh = std::move(mesh);
    ComponentDatas comps;
    comps.push_back(std::move(c));

    Index mid = mgr.addModel("point_global_ids_test", std::move(comps));
    REQUIRE(mid >= 0);

    auto compIds = mgr.modelById(mid)->componentIds();
    REQUIRE(compIds.size() == 1);
    Index cid = compIds[0];

    ComponentData* comp = mgr.findComponent(cid);
    REQUIRE(comp);
    REQUIRE(comp->mesh);

    const MeshData& md = *comp->mesh;

    // 入库后 MeshData 自包含：坐标保留，连通性保持局部点 id
    REQUIRE(md.vertex_positions_.size() == 4);
    REQUIRE(md.vertex_count_ == 4);
    REQUIRE(md.face_vertices_ == std::vector<Index> { 0, 1, 2 });
    REQUIRE(md.edge_vertices_ == std::vector<Index> { 0, 1 });

    // 局部点 id -> gid 已分配，且全局映射回指 (component, local)
    REQUIRE(comp->point_global_ids_.size() == 4);
    for (Index local = 0; local < md.vertex_count_; ++local) {
        const Index gid = comp->point_global_ids_[local];
        REQUIRE(gid >= 0);
        auto [gcid, glocal] = mgr.pointIdMap().getLocal(gid);
        REQUIRE(gcid == cid);
        REQUIRE(glocal == local);
    }

    // 幂等：再次分配不改变既有 gid
    const auto gids_before = comp->point_global_ids_;
    comp->ensurePointGlobalIds(mgr.pointIdMap());
    REQUIRE(comp->point_global_ids_ == gids_before);

    // 运行期加点后补缺：新点获得 gid，旧点 gid 不变
    comp->mesh->vertex_positions_.push_back({ 0, 0, 1 });
    comp->mesh->vertex_count_ = 5;
    comp->ensurePointGlobalIds(mgr.pointIdMap());
    REQUIRE(comp->point_global_ids_.size() == 5);
    for (Index local = 0; local < 4; ++local) {
        REQUIRE(comp->point_global_ids_[local] == gids_before[local]);
    }
    {
        auto [gcid, glocal] = mgr.pointIdMap().getLocal(comp->point_global_ids_[4]);
        REQUIRE(gcid == cid);
        REQUIRE(glocal == 4);
    }

    // 移除组件后回收全部点 gid
    const size_t free_before = mgr.pointIdMap().freeSize();
    mgr.removeComponent(cid);
    REQUIRE(mgr.pointIdMap().freeSize() >= free_before + 5);
}
