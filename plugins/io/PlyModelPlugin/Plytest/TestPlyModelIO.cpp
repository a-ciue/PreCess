// Test for PlyModelHandler: read PLY files, snapshot mesh, commit to ModelLayer, write out, read back and compare
#include "PlyModelHandler.h"

#include "ComponentData.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "ModelPayload.h"
#include "TempFile.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace systems::io;
namespace fs = std::filesystem;

struct MeshSnapshot {
    std::vector<std::array<double, 3>> vertex_positions;
    std::vector<Index> face_vertices;
    std::vector<Index> face_offsets; // size = nFaces+1
};

static MeshData* first_component_mesh(ModelPayload& p)
{
    auto& comps = p.components;
    if (comps.empty() || !comps[0] || !comps[0]->mesh)
        return nullptr;
    return comps[0]->mesh.get();
}

static const MeshData* first_component_mesh(const ModelPayload& p)
{
    const auto& comps = p.components;
    if (comps.empty() || !comps[0] || !comps[0]->mesh)
        return nullptr;
    return comps[0]->mesh.get();
}

static MeshSnapshot snapshot_from_mesh(const MeshData& mesh)
{
    MeshSnapshot s;
    s.vertex_positions = mesh.vertex_positions_;
    s.face_vertices = mesh.face_vertices_;
    s.face_offsets = mesh.face_vertices_offset_;
    return s;
}

static bool snapshot_equal_mesh(const MeshSnapshot& s, const MeshData& mesh, double eps = 1e-6)
{
    if (s.vertex_positions.size() != mesh.vertex_positions_.size())
        return false;
    if (s.face_offsets.size() != mesh.face_vertices_offset_.size())
        return false;
    if (s.face_vertices.size() != mesh.face_vertices_.size())
        return false;

    for (size_t i = 0; i < s.vertex_positions.size(); ++i) {
        for (int k = 0; k < 3; ++k) {
            double a = s.vertex_positions[i][k];
            double b = mesh.vertex_positions_[i][k];
            if (std::fabs(a - b) > eps)
                return false;
        }
    }

    for (size_t i = 0; i < s.face_offsets.size(); ++i) {
        if (s.face_offsets[i] != mesh.face_vertices_offset_[i])
            return false;
    }
    for (size_t i = 0; i < s.face_vertices.size(); ++i) {
        if (s.face_vertices[i] != mesh.face_vertices_[i])
            return false;
    }
    return true;
}

static fs::path test_dir_from_source()
{
    return fs::path(__FILE__).parent_path();
}

TEST_CASE("PlyModelHandler ReadWrite simple.ply (read_model + write_components)")
{
    PlyModelHandler handler;

    fs::path dir = test_dir_from_source();
    fs::path infile = dir / "simple.ply";
    REQUIRE(fs::exists(infile));

    auto payload = handler.read_model(infile, {});
    REQUIRE(payload.has_value());

    MeshData* mesh0 = first_component_mesh(*payload);
    REQUIRE(mesh0 != nullptr);

    REQUIRE(mesh0->vertex_positions_.size() == 5u);
    REQUIRE(mesh0->face_vertices_offset_.size() == 4u); // 0 + 3 faces
    REQUIRE(mesh0->face_vertices_.size() == 3u * 3u);

    MeshSnapshot snap = snapshot_from_mesh(*mesh0);

    ModelLayer mgr;
    Index model_id = mgr.addModel(payload->model_name, std::move(payload->components));
    auto cids = mgr.modelById(model_id)->componentIds();
    REQUIRE(cids.size() == 1);

    fs::path out = core::TempFile::instance().path();
    out.replace_extension(".ply");

    REQUIRE_NOTHROW(handler.write_components(mgr, cids, out, {}));
    REQUIRE(fs::exists(out));

    auto payload2 = handler.read_model(out, {});
    REQUIRE(payload2.has_value());

    const MeshData* mesh2 = first_component_mesh(*payload2);
    REQUIRE(mesh2 != nullptr);

    REQUIRE(snapshot_equal_mesh(snap, *mesh2));
}

TEST_CASE("PlyModelHandler ReadWrite test.ply (read_model + write_components)")
{
    PlyModelHandler handler;

    fs::path dir = test_dir_from_source();
    fs::path infile = dir / "test.ply";
    REQUIRE(fs::exists(infile));

    auto payload = handler.read_model(infile, {});
    REQUIRE(payload.has_value());

    MeshData* mesh0 = first_component_mesh(*payload);
    REQUIRE(mesh0 != nullptr);

    REQUIRE(mesh0->vertex_positions_.size() == 10u);
    REQUIRE(mesh0->face_vertices_offset_.size() == 8u); // 0 + 7 faces

    MeshSnapshot snap = snapshot_from_mesh(*mesh0);

    ModelLayer mgr;
    Index model_id = mgr.addModel(payload->model_name, std::move(payload->components));
    auto cids = mgr.modelById(model_id)->componentIds();
    REQUIRE(cids.size() == 1);

    fs::path out = core::TempFile::instance().path();
    out.replace_extension(".ply");

    REQUIRE_NOTHROW(handler.write_components(mgr, cids, out, {}));
    REQUIRE(fs::exists(out));

    auto payload2 = handler.read_model(out, {});
    REQUIRE(payload2.has_value());

    const MeshData* mesh2 = first_component_mesh(*payload2);
    REQUIRE(mesh2 != nullptr);

    REQUIRE(snapshot_equal_mesh(snap, *mesh2));
}
