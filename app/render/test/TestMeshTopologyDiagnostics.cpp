#include "MeshData.h"
#include "MeshTopologyDiagnostics.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {
//! @brief 仅引用拓扑诊断需要的网格数据，不引入 MeshActor 和 VTK 渲染类型
MeshDataVtk makeDiagnosticMeshData(const MeshData& mesh)
{
    return {
        mesh.solid_types_,
        mesh.solid_vertices_,
        mesh.solid_vertices_offset_,
        mesh.solid_faces_vertices_,
        mesh.solid_faces_vertices_offset_,
        mesh.solid_faces_,
        mesh.solid_faces_offset_,
        mesh.face_vertices_,
        mesh.face_vertices_offset_,
        mesh.edge_vertices_,
        mesh.vertex_positions_,
        mesh.vertex_attributes_,
        mesh.edge_attributes_,
        mesh.face_attributes_,
        mesh.solid_attributes_,
        {},
        -1
    };
}

//! @brief 将测试网格转换为渲染数据并执行拓扑诊断
MeshTopologyDiagnosticResult analyzeMesh(const MeshData& mesh)
{
    const MeshDataVtk model_data = makeDiagnosticMeshData(mesh);
    return MeshTopologyDiagnostics::analyze(model_data);
}
}

TEST_CASE("MeshTopologyDiagnostics classifies boundary and dihedral edges")
{
    MeshData mesh;
    mesh.vertex_positions_ = {
        { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 },
        { 1.0, 1.0, 0.0 }, { 0.0, 1.0, 0.0 }
    };
    mesh.face_vertices_ = { 0, 1, 2, 0, 2, 3 };
    mesh.face_vertices_offset_ = { 0, 3, 6 };

    const MeshTopologyDiagnosticResult result = analyzeMesh(mesh);

    REQUIRE(result.boundary_edges.size() == 4);
    REQUIRE(result.boundary_faces.size() == 2);
    REQUIRE(result.manifold_edges.size() == 1);
    REQUIRE(result.manifold_edges.front().dihedral_angle_degrees == Catch::Approx(180.0));
    REQUIRE(result.non_manifold_edges.empty());
    REQUIRE(result.non_manifold_vertices.empty());
}

TEST_CASE("MeshTopologyDiagnostics computes a right interior dihedral angle")
{
    MeshData mesh;
    mesh.vertex_positions_ = {
        { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 },
        { 0.0, 1.0, 0.0 }, { 0.0, 0.0, 1.0 }
    };
    // 两个正交三角形共享边 (0,1)，内二面角为 90°。
    mesh.face_vertices_ = { 0, 1, 2, 1, 0, 3 };
    mesh.face_vertices_offset_ = { 0, 3, 6 };

    const MeshTopologyDiagnosticResult result = analyzeMesh(mesh);

    REQUIRE(result.manifold_edges.size() == 1);
    REQUIRE(result.manifold_edges.front().dihedral_angle_degrees == Catch::Approx(90.0));
}

TEST_CASE("MeshTopologyDiagnostics excludes non manifold edge endpoints from non manifold vertices")
{
    MeshData mesh;
    mesh.vertex_positions_ = {
        { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 1.0 }, { 0.0, -1.0, 0.0 }
    };
    // 三个三角形共同使用边 (0,1)。
    mesh.face_vertices_ = { 0, 1, 2, 1, 0, 3, 0, 1, 4 };
    mesh.face_vertices_offset_ = { 0, 3, 6, 9 };

    const MeshTopologyDiagnosticResult result = analyzeMesh(mesh);

    REQUIRE(result.non_manifold_edges.size() == 1);
    REQUIRE(result.non_manifold_vertices.empty());
}

TEST_CASE("MeshTopologyDiagnostics classifies isolated edge and vertex")
{
    MeshData mesh;
    mesh.vertex_positions_ = {
        { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 2.0, 0.0, 0.0 }
    };
    mesh.edge_vertices_ = { 0, 1 };
    mesh.face_vertices_offset_ = { 0 };

    const MeshTopologyDiagnosticResult result = analyzeMesh(mesh);

    REQUIRE(result.isolated_edges.size() == 1);
    REQUIRE(result.isolated_vertices == std::vector<Index> { 2 });
}

TEST_CASE("MeshTopologyDiagnostics detects a disconnected vertex fan")
{
    MeshData mesh;
    mesh.vertex_positions_ = {
        { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 },
        { -1.0, 0.0, 0.0 }, { 0.0, -1.0, 0.0 }
    };
    // 两个三角形只共享点 0，其一环由两个互不连通的面扇组成。
    mesh.face_vertices_ = { 0, 1, 2, 0, 3, 4 };
    mesh.face_vertices_offset_ = { 0, 3, 6 };

    const MeshTopologyDiagnosticResult result = analyzeMesh(mesh);

    REQUIRE(result.non_manifold_edges.empty());
    REQUIRE(result.non_manifold_vertices == std::vector<Index> { 0 });
}
