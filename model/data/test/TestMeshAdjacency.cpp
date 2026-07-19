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

TEST_CASE("MeshAdjacency indexes face edges and merges shared edges")
{
    // 两个三角形面共享边 (0,2)
    MeshData mesh;
    mesh.face_vertices_ = { 0, 1, 2, 0, 2, 3 };
    mesh.face_vertices_offset_ = { 0, 3, 6 };

    MeshAdjacency adj;

    // 面边可反查：面0边 (0,1)(1,2)(2,0)，面1边 (0,2)(2,3)(3,0)
    REQUIRE(adj.findEdgeByEndpoints(mesh, 0, 1).value() == 0);
    REQUIRE(adj.findEdgeByEndpoints(mesh, 2, 0).value() == 2);
    // 共享边 (0,2) 与 (2,0) 归并到同一行
    REQUIRE(adj.findEdgeByEndpoints(mesh, 0, 2).value() == 2);
    // 5 条几何边（共享边只算一次），全部未物化
    REQUIRE(adj.edgeCount(mesh) == 5);
    REQUIRE(adj.edgeCellIndex(mesh, 2) == -1);

    // faceEdgeRows 与 face_vertices_ 等长对齐，共享边在两面上行号一致
    const auto& fers = adj.faceEdgeRows(mesh);
    REQUIRE(fers.size() == mesh.face_vertices_.size());
    REQUIRE(fers[2] == fers[3]); // 面0第3条边 (2,0) == 面1第1条边 (0,2)
    REQUIRE(fers[0] == 0);
    REQUIRE(fers[1] == 1);
}

TEST_CASE("MeshAdjacency merges coincident face edge into materialized edge row")
{
    // 物化边 (0,1)，同时是面 (0,1,2) 的一条边：重合归并，面边获得物化边的行
    MeshData mesh;
    mesh.edge_vertices_ = { 0, 1 };
    mesh.face_vertices_ = { 0, 1, 2 };
    mesh.face_vertices_offset_ = { 0, 3 };

    MeshAdjacency adj;

    const auto row = adj.findEdgeByEndpoints(mesh, 1, 0);
    REQUIRE(row.has_value());
    REQUIRE(*row == 0); // 物化边先灌入，行号 0
    REQUIRE(adj.edgeCellIndex(mesh, *row) == 0); // 行携 cell 序号

    // 面边反查命中同一行；纯面边 (1,2) 未物化
    REQUIRE(adj.faceEdgeRows(mesh)[0] == *row);
    REQUIRE(adj.edgeCellIndex(mesh, adj.findEdgeByEndpoints(mesh, 1, 2).value()) == -1);
    REQUIRE(adj.edgeCount(mesh) == 3);
}

TEST_CASE("MeshAdjacency collapses duplicated edge cells, first cell wins")
{
    MeshData mesh;
    mesh.edge_vertices_ = { 0, 1, 0, 1 };

    MeshAdjacency adj;

    REQUIRE(adj.edgeCount(mesh) == 1);
    const auto row = adj.findEdgeByEndpoints(mesh, 0, 1);
    REQUIRE(row.has_value());
    REQUIRE(adj.edgeCellIndex(mesh, *row) == 0);
}
