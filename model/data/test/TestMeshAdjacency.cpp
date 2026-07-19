#include "MeshAdjacency.h"
#include "MeshData.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("MeshAdjacency findEdgeByEndpoints resolves standalone edges")
{
    MeshData mesh;
    mesh.edge_vertices_ = { 0, 1, 1, 2, 5, 7 };

    MeshAdjacency adj;

    auto e0 = adj.findEdgeByEndpoints(mesh, 0, 1);
    REQUIRE(e0.has_value());
    REQUIRE(*e0 == 0);

    // 端点交换亦命中同一条边
    auto e1 = adj.findEdgeByEndpoints(mesh, 2, 1);
    REQUIRE(e1.has_value());
    REQUIRE(*e1 == 1);

    auto e2 = adj.findEdgeByEndpoints(mesh, 7, 5);
    REQUIRE(e2.has_value());
    REQUIRE(*e2 == 2);
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
    REQUIRE(*e1 == 1);
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
