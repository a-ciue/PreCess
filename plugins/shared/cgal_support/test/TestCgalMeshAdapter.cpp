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

    const CgalMesh sm = toSurfaceMesh(src, {});
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

    REQUIRE_THROWS_AS(toSurfaceMesh(mesh, {}), std::runtime_error);
}

TEST_CASE("CgalMeshAdapter converts global vertex ids via local_to_global", "[cgal_support]")
{
    // IO 组件约定：local_to_global_ 非空且 vertex_positions_ 已清空（坐标存于全局点数组），
    // face_vertices_ 存全局点 id
    MeshData mesh = makeTriangleMesh();
    const auto positions = mesh.vertex_positions_; // 暂存坐标用于构造全局点数组
    mesh.local_to_global_ = { 10, 11, 12, 13 };
    mesh.face_vertices_ = { 10, 11, 12, 10, 12, 13 };
    mesh.vertex_positions_.clear(); // 全局约定下局部坐标已由 ModelLayer 清空
    // 全局点数组：组件坐标放在 id 10-13 处（模拟 ModelLayer 的全局点布局）
    std::vector<std::array<double, 3>> global_points(14, { 0.0, 0.0, 0.0 });
    for (size_t i = 0; i < positions.size(); ++i)
        global_points[10 + i] = positions[i];

    const CgalMesh sm = toSurfaceMesh(mesh, global_points);
    REQUIRE(sm.number_of_vertices() == 4);
    REQUIRE(sm.number_of_faces() == 2);

    // 回转后为局部索引约定，local_to_global_ 清空
    MeshData back;
    fromSurfaceMesh(sm, back);
    REQUIRE(back.vertex_positions_ == positions);
    REQUIRE(back.face_vertices_ == std::vector<Index> { 0, 1, 2, 0, 2, 3 });
    REQUIRE(back.local_to_global_.empty());
}

TEST_CASE("CgalMeshAdapter throws on global id outside the component", "[cgal_support]")
{
    MeshData mesh = makeTriangleMesh();
    mesh.local_to_global_ = { 10, 11, 12, 13 };
    mesh.face_vertices_ = { 10, 11, 99, 10, 12, 13 }; // 99 不在组件内
    mesh.vertex_positions_.clear(); // 全局约定下局部坐标已清空

    REQUIRE_THROWS_AS(toSurfaceMesh(mesh, std::vector<std::array<double, 3>>(14, { 0.0, 0.0, 0.0 })),
        std::runtime_error);
}
