#include <catch2/catch_test_macros.hpp>
#include "ModelLayer.h"
#include "ModelData.h"
#include "MeshData.h"
#include "ModelObserver.h"

struct DummyObserver : ModelObserver {
    void notifyModelChanged(Index) override {}
    void notifyModelAdded(Index) override {}
    void notifyModelRemoved(Index) override {}
    void notifyComponentRemoved(Index) override {}
    void notifyModelNameChanged(Index, const std::string&) override {}
    void notifySplineLoadFailed(const std::string&) override {}
};

TEST_CASE("Global points pool + globalized indices + vertex_positions swapped out", "[GlobalPoints]")
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

    auto model = std::make_unique<ModelData>(std::move(mesh));
    Index mid = mgr.addModel(std::move(model));
    REQUIRE(mid >= 0);

    auto compIds = mgr.getComponentIds(mid);
    REQUIRE(compIds.size() == 1);
    Index cid = compIds[0];

    ComponentData* c = mgr.findComponent(cid);
    REQUIRE(c);
    REQUIRE(c->mesh);

    const MeshData& md = *c->mesh;

    REQUIRE(md.vertex_positions_.empty());
    REQUIRE(md.point_ids_are_global_);
    REQUIRE(md.global_point_base_ >= 0);
    REQUIRE(md.vertex_count_ == 4);
    REQUIRE((int)mgr.globalPoints().size() >= md.global_point_base_ + md.vertex_count_);

    // indices 应该已经全局化：原来 0.. 现在 base+0..
    REQUIRE(md.face_vertices_[0] == md.global_point_base_ + 0);
    REQUIRE(md.face_vertices_[1] == md.global_point_base_ + 1);
    REQUIRE(md.face_vertices_[2] == md.global_point_base_ + 2);

    REQUIRE(md.edge_vertices_[0] == md.global_point_base_ + 0);
    REQUIRE(md.edge_vertices_[1] == md.global_point_base_ + 1);

    // 所有点索引不能越界 globalPoints
    auto check = [&](const std::vector<Index>& a){
        for (Index v: a) {REQUIRE(v >= 0); REQUIRE(v < (Index)mgr.globalPoints().size());
    } };
    check(md.face_vertices_);
    check(md.edge_vertices_);
}