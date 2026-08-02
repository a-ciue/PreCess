#include "ComponentData.h"
#include "MeshData.h"
#include <catch2/catch_test_macros.hpp>

static bool ownsLocalPoint(const ComponentData& c, Index local_pid)
{
    if (!c.mesh)
        return false;
    return local_pid >= 0 && local_pid < c.mesh->vertex_count_;
}

TEST_CASE("ComponentData Geometry edge -> mesh point ids map", "[GeometryMeshMap]")
{
    ComponentData c;
    c.id = 1;
    c.mesh = std::make_unique<MeshData>();
    c.mesh->vertex_positions_ = {
        { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 }, { 0, 0, 1 }
    };
    c.mesh->vertex_count_ = 5;

    GeomEdgeId e0 = 7;
    std::vector<Index> pts = { 0, 1, 2 };

    // ensure mapping created on demand
    c.ensureMapping().geometry_edge_to_mesh_point_ids[e0] = pts;

    REQUIRE(c.mapping);
    REQUIRE(c.mapping->geometry_edge_to_mesh_point_ids.count(e0) == 1);
    REQUIRE(c.mapping->geometry_edge_to_mesh_point_ids[e0].size() == 3);

    // optional: validate ownership helper
    REQUIRE(ownsLocalPoint(c, 2));
    REQUIRE_FALSE(ownsLocalPoint(c, -1));
    REQUIRE_FALSE(ownsLocalPoint(c, 5));
}
