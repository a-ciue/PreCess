#include "ComponentData.h"
#include "MeshData.h"
#include <catch2/catch_test_macros.hpp>

static bool ownsGlobalPoint(const ComponentData& c, Index global_pid)
{
    if (!c.mesh)
        return false;
    const Index base = c.mesh->global_point_base_;
    const Index cnt = c.mesh->vertex_count_;
    return base >= 0 && cnt >= 0 && global_pid >= base && global_pid < base + cnt;
}

TEST_CASE("ComponentData Geometry edge -> mesh point gids map", "[GeometryMeshMap]")
{
    ComponentData c;
    c.id = 1;
    c.mesh = std::make_unique<MeshData>();
    c.mesh->global_point_base_ = 10;
    c.mesh->vertex_count_ = 5; // points are [10..15)

    GeomEdgeId e0 = 7;
    std::vector<Index> pts = { 10, 11, 12 };

    // ensure mapping created on demand
    c.ensureMapping().geometry_edge_to_mesh_point_gids[e0] = pts;

    REQUIRE(c.mapping);
    REQUIRE(c.mapping->geometry_edge_to_mesh_point_gids.count(e0) == 1);
    REQUIRE(c.mapping->geometry_edge_to_mesh_point_gids[e0].size() == 3);

    // optional: validate ownership helper
    REQUIRE(ownsGlobalPoint(c,10));
    REQUIRE_FALSE(ownsGlobalPoint(c,9));
    REQUIRE_FALSE(ownsGlobalPoint(c,15));
}