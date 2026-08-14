#include "MeshTopologyDiagnostics.h"

#include "MeshAdjacency.h"
#include "MeshData.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("MeshTopologyDiagnostics classifies boundary and dihedral edges")
{
    MeshData mesh;
    mesh.vertex_positions_ = {
        { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 },
        { 1.0, 1.0, 0.0 }, { 0.0, 1.0, 0.0 }
    };
    mesh.face_vertices_ = { 0, 1, 2, 0, 2, 3 };
    mesh.face_vertices_offset_ = { 0, 3, 6 };

    MeshAdjacency adjacency;
    const MeshTopologyDiagnosticResult result = MeshTopologyDiagnostics::analyze(adjacency, mesh);

    REQUIRE(result.boundary_edges.size() == 4);
    REQUIRE(result.boundary_faces.size() == 2);
    REQUIRE(result.manifold_edges.size() == 1);
    REQUIRE(result.manifold_edges.front().dihedral_angle_degrees == Catch::Approx(0.0));
    REQUIRE(result.non_manifold_edges.empty());
    REQUIRE(result.non_manifold_vertices.empty());
}

TEST_CASE("MeshTopologyDiagnostics classifies non manifold edge and vertices")
{
    MeshData mesh;
    mesh.vertex_positions_ = {
        { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 1.0 }, { 0.0, -1.0, 0.0 }
    };
    // 三个三角形共同使用边 (0,1)。
    mesh.face_vertices_ = { 0, 1, 2, 1, 0, 3, 0, 1, 4 };
    mesh.face_vertices_offset_ = { 0, 3, 6, 9 };

    MeshAdjacency adjacency;
    const MeshTopologyDiagnosticResult result = MeshTopologyDiagnostics::analyze(adjacency, mesh);

    REQUIRE(result.non_manifold_edges.size() == 1);
    REQUIRE(result.non_manifold_vertices.size() == 2);
}

TEST_CASE("MeshTopologyDiagnostics classifies isolated edge and vertex")
{
    MeshData mesh;
    mesh.vertex_positions_ = {
        { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 2.0, 0.0, 0.0 }
    };
    mesh.edge_vertices_ = { 0, 1 };
    mesh.face_vertices_offset_ = { 0 };

    MeshAdjacency adjacency;
    const MeshTopologyDiagnosticResult result = MeshTopologyDiagnostics::analyze(adjacency, mesh);

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

    MeshAdjacency adjacency;
    const MeshTopologyDiagnosticResult result = MeshTopologyDiagnostics::analyze(adjacency, mesh);

    REQUIRE(result.non_manifold_edges.empty());
    REQUIRE(result.non_manifold_vertices == std::vector<Index> { 0 });
}
