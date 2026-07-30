#include "MeshAdjacency.h"
#include "MeshData.h"
#include "MeshIDMap.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("MeshAdjacency findEdgeByEndpoints resolves standalone edges")
{
    MeshData mesh;
    mesh.edge_vertices_ = { 0, 1, 1, 2, 5, 7 };

    MeshAdjacency adj;

    auto e0 = adj.findEdgeByEndpoints(mesh, 0, 1);
    REQUIRE(e0.has_value());
    REQUIRE(adj.edgeStableId(mesh, *e0) == 0);

    // 端点交换亦命中同一条边
    auto e1 = adj.findEdgeByEndpoints(mesh, 2, 1);
    REQUIRE(e1.has_value());
    REQUIRE(adj.edgeStableId(mesh, *e1) == 1);

    auto e2 = adj.findEdgeByEndpoints(mesh, 7, 5);
    REQUIRE(e2.has_value());
    REQUIRE(adj.edgeStableId(mesh, *e2) == 2);
}

TEST_CASE("MeshAdjacency findEdgeByEndpoints misses unknown pairs")
{
    MeshData mesh;
    mesh.edge_vertices_ = { 0, 1, 1, 2 };

    MeshAdjacency adj;

    REQUIRE_FALSE(adj.findEdgeByEndpoints(mesh, 0, 2).has_value());
    // 非法端点 id 直接未命中
    REQUIRE_FALSE(adj.findEdgeByEndpoints(mesh, -1, 1).has_value());
}

TEST_CASE("MeshAdjacency rebuilds after invalidate")
{
    MeshData mesh;
    mesh.edge_vertices_ = { 0, 1 };

    MeshAdjacency adj;
    REQUIRE(adj.findEdgeByEndpoints(mesh, 0, 1).has_value());

    // 拓扑变更后 invalidate，新增边即可被查
    mesh.edge_vertices_ = { 0, 1, 3, 4 };
    adj.invalidate();

    auto e1 = adj.findEdgeByEndpoints(mesh, 4, 3);
    REQUIRE(e1.has_value());
    REQUIRE(adj.edgeStableId(mesh, *e1) == 1);
}

TEST_CASE("MeshAdjacency handles empty and malformed edge data")
{
    MeshAdjacency adj;

    MeshData empty_mesh;
    REQUIRE_FALSE(adj.findEdgeByEndpoints(empty_mesh, 0, 1).has_value());

    // 奇数长度的 edge_vertices_ 为异常数据，按无有效边处理（不崩溃）
    MeshData bad_mesh;
    bad_mesh.edge_vertices_ = { 0, 1, 2 };
    REQUIRE_FALSE(adj.findEdgeByEndpoints(bad_mesh, 0, 1).has_value());
}

TEST_CASE("MeshAdjacency indexes face edges and merges shared edges")
{
    // 两个三角形面共享边 (0,2)
    MeshData mesh;
    mesh.face_vertices_ = { 0, 1, 2, 0, 2, 3 };
    mesh.face_vertices_offset_ = { 0, 3, 6 };

    MeshAdjacency adj;

    // 面边可反查，共享边 (0,2)/(2,0) 归并到同一稳定 id
    const auto sid_shared_a = adj.edgeStableId(mesh, adj.findEdgeByEndpoints(mesh, 2, 0).value());
    const auto sid_shared_b = adj.edgeStableId(mesh, adj.findEdgeByEndpoints(mesh, 0, 2).value());
    REQUIRE(sid_shared_a.has_value());
    REQUIRE(sid_shared_a == sid_shared_b);

    // 5 条几何边（共享边只算一次），全部未物化
    REQUIRE(adj.edgeCount(mesh) == 5);
    REQUIRE(adj.edgeCellIndex(mesh, adj.findEdgeByEndpoints(mesh, 0, 2).value()) == -1);

    // faceEdgeStableIds 与 face_vertices_ 等长对齐，共享边在两面上稳定 id 一致
    const auto& fess = adj.faceEdgeStableIds(mesh);
    REQUIRE(fess.size() == mesh.face_vertices_.size());
    REQUIRE(fess[2] == fess[3]); // 面0第3条边 (2,0) == 面1第1条边 (0,2)
    REQUIRE(fess[2] == sid_shared_a);
}

TEST_CASE("MeshAdjacency merges coincident face edge into materialized edge row")
{
    // 物化边 (0,1)，同时是面 (0,1,2) 的一条边：重合归并，面边获得物化边的身份
    MeshData mesh;
    mesh.edge_vertices_ = { 0, 1 };
    mesh.face_vertices_ = { 0, 1, 2 };
    mesh.face_vertices_offset_ = { 0, 3 };

    MeshAdjacency adj;

    const auto edge = adj.findEdgeByEndpoints(mesh, 1, 0);
    REQUIRE(edge.has_value());
    REQUIRE(adj.edgeCellIndex(mesh, *edge) == 0); // 携物化 cell 序号
    const auto sid = adj.edgeStableId(mesh, *edge);
    REQUIRE(sid.has_value());

    // 面边反查命中同一身份；纯面边 (1,2) 未物化
    REQUIRE(adj.faceEdgeStableIds(mesh)[0] == sid);
    REQUIRE(adj.edgeCellIndex(mesh, adj.findEdgeByEndpoints(mesh, 1, 2).value()) == -1);
    REQUIRE(adj.edgeCount(mesh) == 3);
}

TEST_CASE("MeshAdjacency collapses duplicated edge cells, first cell wins")
{
    MeshData mesh;
    mesh.edge_vertices_ = { 0, 1, 0, 1 };

    MeshAdjacency adj;

    REQUIRE(adj.edgeCount(mesh) == 1);
    const auto edge = adj.findEdgeByEndpoints(mesh, 0, 1);
    REQUIRE(edge.has_value());
    REQUIRE(adj.edgeCellIndex(mesh, *edge) == 0);
}

TEST_CASE("MeshAdjacency stable ids survive topology edits")
{
    // 两个三角形面共享边 (0,2)，另有独立物化边 (4,5)
    MeshData mesh;
    mesh.edge_vertices_ = { 4, 5 };
    mesh.face_vertices_ = { 0, 1, 2, 0, 2, 3 };
    mesh.face_vertices_offset_ = { 0, 3, 6 };

    MeshAdjacency adj;

    const auto sid_standalone = adj.edgeStableId(mesh, adj.findEdgeByEndpoints(mesh, 4, 5).value());
    const auto sid_shared = adj.edgeStableId(mesh, adj.findEdgeByEndpoints(mesh, 0, 2).value());
    const auto sid_f01 = adj.edgeStableId(mesh, adj.findEdgeByEndpoints(mesh, 0, 1).value());
    const auto sid_dead = adj.edgeStableId(mesh, adj.findEdgeByEndpoints(mesh, 2, 3).value());
    REQUIRE(sid_standalone.has_value());
    REQUIRE(sid_shared.has_value());
    REQUIRE(sid_f01.has_value());
    REQUIRE(sid_dead.has_value());

    // 拓扑编辑：删除第二个面（(2,3) 随之消亡），新增一条物化边 (6,7)
    mesh.face_vertices_ = { 0, 1, 2 };
    mesh.face_vertices_offset_ = { 0, 3 };
    mesh.edge_vertices_ = { 4, 5, 6, 7 };
    adj.invalidate();

    // 存活的边：稳定 id 不变，且可经稳定 id 重新签发句柄
    REQUIRE(adj.edgeStableId(mesh, adj.findEdgeByEndpoints(mesh, 2, 0).value()) == sid_shared);
    REQUIRE(adj.edgeStableId(mesh, adj.findEdgeByEndpoints(mesh, 4, 5).value()) == sid_standalone);
    REQUIRE(adj.edgeStableId(mesh, adj.findEdgeByEndpoints(mesh, 0, 1).value()) == sid_f01);
    REQUIRE(adj.findEdgeByStableId(mesh, *sid_shared).has_value());

    // 消亡的边：无法签发句柄
    REQUIRE_FALSE(adj.findEdgeByStableId(mesh, *sid_dead).has_value());

    // 新增边获得全新稳定 id（不复用旧值）
    const auto sid_new = adj.edgeStableId(mesh, adj.findEdgeByEndpoints(mesh, 6, 7).value());
    REQUIRE(sid_new.has_value());
    REQUIRE(*sid_new != *sid_standalone);
    REQUIRE(*sid_new != *sid_shared);
    REQUIRE(*sid_new != *sid_f01);
    REQUIRE(*sid_new != *sid_dead);
}

TEST_CASE("MeshAdjacency assigns and releases global edge ids")
{
    MeshIDMap map;
    const Index component_id = 7;

    MeshData mesh;
    mesh.edge_vertices_ = { 0, 1 };
    mesh.face_vertices_ = { 0, 1, 2 };
    mesh.face_vertices_offset_ = { 0, 3 };

    MeshAdjacency adj;

    // 分配前查询为空
    const Index sid0 = adj.edgeStableId(mesh, adj.findEdgeByEndpoints(mesh, 0, 1).value()).value();
    REQUIRE_FALSE(adj.edgeGlobalId(mesh, sid0).has_value());

    // 分配：物化边与纯面边都有 gid，且全局映射回指 (component, sid)
    adj.ensureEdgeGlobalIds(map, component_id, mesh);
    REQUIRE(map.size() == 3);
    REQUIRE(map.freeSize() == 0);
    for (Index sid = 0; sid < 3; ++sid) {
        auto gid = adj.edgeGlobalId(mesh, sid);
        REQUIRE(gid.has_value());
        auto [cid, lid] = map.getLocal(*gid);
        REQUIRE(cid == component_id);
        REQUIRE(lid == sid);
    }

    // 幂等：再次分配不新增 gid
    adj.ensureEdgeGlobalIds(map, component_id, mesh);
    REQUIRE(map.size() == 3);

    // 释放：全部 gid 入复用池，持久层重置（sid 重新从 0 起）
    adj.releaseEdgeGlobalIds(map);
    REQUIRE(map.freeSize() == 3);
    REQUIRE(adj.edgeStableId(mesh, adj.findEdgeByEndpoints(mesh, 0, 1).value()).value() == 0);
}

TEST_CASE("MeshAdjacency edge handles expire after rebuild")
{
    MeshData mesh;
    mesh.edge_vertices_ = { 0, 1 };

    MeshAdjacency adj;
    const auto edge = adj.findEdgeByEndpoints(mesh, 0, 1);
    REQUIRE(edge.has_value());
    const auto sid = adj.edgeStableId(mesh, *edge);
    REQUIRE(sid.has_value());

    // 边表重建后旧句柄失效：不可换算稳定 id、不可查 cell
    adj.invalidate();
    mesh.edge_vertices_ = { 0, 1, 2, 3 };
    REQUIRE(adj.findEdgeByEndpoints(mesh, 2, 3).has_value()); // 触发重建
    REQUIRE_FALSE(adj.edgeStableId(mesh, *edge).has_value());
    REQUIRE(adj.edgeCellIndex(mesh, *edge) == -1);

    // 同一条边可重新签发句柄，稳定 id 不变
    const auto edge2 = adj.findEdgeByEndpoints(mesh, 0, 1);
    REQUIRE(edge2.has_value());
    REQUIRE(adj.edgeStableId(mesh, *edge2) == sid);
}
