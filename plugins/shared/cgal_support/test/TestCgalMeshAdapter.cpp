/**
 * @file TestCgalMeshAdapter.cpp
 * @brief CgalMeshAdapter 转换层的单元测试
 */

#include "CgalMeshAdapter.h"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

namespace {
//! @brief 两个三角面构成的单位正方形平面（4 顶点 2 面）
MeshData makeTriangleMesh()
{
    MeshData mesh;
    mesh.vertex_positions_ = {
        { 0.0, 0.0, 0.0 },
        { 1.0, 0.0, 0.0 },
        { 1.0, 1.0, 0.0 },
        { 0.0, 1.0, 0.0 },
    };
    mesh.vertex_count_ = 4;
    mesh.face_vertices_ = { 0, 1, 2, 0, 2, 3 };
    mesh.face_vertices_offset_ = { 0, 3, 6 };
    return mesh;
}
}

TEST_CASE("CgalMeshAdapter round-trip conversion is consistent", "[cgal_support]")
{
    const MeshData src = makeTriangleMesh();

    const CgalMesh sm = toSurfaceMesh(src);
    REQUIRE(sm.number_of_vertices() == src.vertex_positions_.size());
    REQUIRE(sm.number_of_faces() == 2);

    MeshData back;
    fromSurfaceMesh(sm, back);
    REQUIRE(back.vertex_positions_ == src.vertex_positions_);
    REQUIRE(back.face_vertices_offset_ == src.face_vertices_offset_);
    REQUIRE(back.face_vertices_ == src.face_vertices_);
}

TEST_CASE("CgalMeshAdapter throws on non-triangle faces", "[cgal_support]")
{
    MeshData mesh = makeTriangleMesh();
    // 合并为一个四边面
    mesh.face_vertices_ = { 0, 1, 2, 3 };
    mesh.face_vertices_offset_ = { 0, 4 };

    REQUIRE_THROWS_AS(toSurfaceMesh(mesh), std::runtime_error);
}

TEST_CASE("CgalMeshAdapter passes local vertex indices through", "[cgal_support]")
{
    // 局部约定：坐标直读 vertex_positions_，face_vertices_ 即组件内局部点索引
    MeshData mesh = makeTriangleMesh();
    mesh.face_vertices_ = { 3, 2, 0, 2, 1, 0 }; // 与顶点顺序不同的局部索引引用

    const CgalMesh sm = toSurfaceMesh(mesh);
    REQUIRE(sm.number_of_vertices() == 4);
    REQUIRE(sm.number_of_faces() == 2);

    // 回转后仍为局部索引约定，逐面顶点索引保持不变
    MeshData back;
    fromSurfaceMesh(sm, back);
    REQUIRE(back.vertex_positions_ == mesh.vertex_positions_);
    REQUIRE(back.face_vertices_ == std::vector<Index> { 3, 2, 0, 2, 1, 0 });
}

TEST_CASE("CgalMeshAdapter throws on local vertex index out of range", "[cgal_support]")
{
    MeshData mesh = makeTriangleMesh();
    mesh.face_vertices_ = { 0, 1, 99, 0, 2, 3 }; // 99 超出局部点索引范围

    REQUIRE_THROWS_AS(toSurfaceMesh(mesh), std::runtime_error);
}
