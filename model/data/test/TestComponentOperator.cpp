#include <catch2/catch_test_macros.hpp>

#include "MeshData.h"
#include "ModelData.h"
#include "ModelLayer.h"
#include "ModelObserver.h"
#include "ComponentOperator.h"

struct DummyObserver : ModelObserver {
    void notifyModelChanged(Index) override { }
    void notifyModelAdded(Index) override { }
    void notifyModelRemoved(Index) override { }
    void notifyComponentRemoved(Index) override { }
    void notifyModelNameChanged(Index, const std::string&) override { }
    void notifySplineLoadFailed(const std::string&) override { }
};

TEST_CASE("Edge global id map is built for true 1D edges", "[MeshIDMap][MeshData]")
{
    DummyObserver obs;
    ModelLayer mgr(&obs);

    auto mesh = std::make_unique<MeshData>();
    mesh->init();

    mesh->vertex_positions_ = {
        { 0, 0, 0 },
        { 1, 0, 0 },
        { 1, 1, 0 },
    };

    // 真实线单元：两条边 (0-1), (1-2)
    mesh->edge_vertices_ = { 0, 1, 1, 2 };

    auto model = std::make_unique<ModelData>(std::move(mesh));
    Index model_id = mgr.addModel(std::move(model));
    REQUIRE(model_id >= 0);

    auto comp_ids = mgr.getComponentIds(model_id);
    REQUIRE(comp_ids.size() == 1);
    Index component_id = comp_ids[0];

    ComponentData* c = mgr.findComponent(component_id);
    REQUIRE(c);
    REQUIRE(c->mesh);

    auto& md = *c->mesh;
    REQUIRE(md.edge_vertices_.size() == 4);
    REQUIRE(md.local_to_global_edge_id.size() == 2);

    for (Index local_eid = 0; local_eid < (Index)md.local_to_global_edge_id.size(); ++local_eid) {
        Index gid = md.local_to_global_edge_id[local_eid];
        REQUIRE(gid >= 0);

        auto [cid, lid] = mgr.edgeIdMap().getLocal(gid);
        REQUIRE(cid == component_id);
        REQUIRE(lid == local_eid);
    }

    // 删除 component 后回收 gid
    const size_t free_before = mgr.edgeIdMap().freeSize();
    mgr.removeComponent(component_id);
    const size_t free_after = mgr.edgeIdMap().freeSize();
    REQUIRE(free_after >= free_before + 2);
}