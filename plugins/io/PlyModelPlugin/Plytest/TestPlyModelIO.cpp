// Test for PlyModelHandler: read PLY files, verify mesh content, write out and verify written file
#include "PlyModelHandler.h"
#include "ComponentData.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "ModelPayload.h"
#include "TempFile.h"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <string>
#include <cmath>
#include <optional>

using namespace systems::io;
namespace fs = std::filesystem;

namespace {
static bool meshes_equal(const MeshData& a, const MeshData& b, double eps = 1e-6)
{
    if (a.vertex_positions_.size() != b.vertex_positions_.size()) return false;
    if (a.face_vertices_offset_.size() != b.face_vertices_offset_.size()) return false;
    if (a.face_vertices_.size() != b.face_vertices_.size()) return false;

    for (size_t i = 0; i < a.vertex_positions_.size(); ++i) {
        for (int k = 0; k < 3; ++k) {
            double va = a.vertex_positions_[i][k];
            double vb = b.vertex_positions_[i][k];
            if (std::fabs(va - vb) > eps) return false;
        }
    }

    for (size_t i = 0; i < a.face_vertices_offset_.size(); ++i) {
        if (a.face_vertices_offset_[i] != b.face_vertices_offset_[i]) return false;
    }

    for (size_t i = 0; i < a.face_vertices_.size(); ++i) {
        if (a.face_vertices_[i] != b.face_vertices_[i]) return false;
    }

    return true;
}

static fs::path test_dir_from_source()
{
    fs::path p = fs::path(__FILE__).parent_path();
    return p;
}

static std::vector<Index> addMeshModelAndGetComponentIds(
    ModelLayer& layer,
    std::unique_ptr<MeshData> mesh)
{
    ComponentDatas comps;
    auto comp = std::make_unique<ComponentData>();
    comp->id = -1;
    comp->mesh = std::move(mesh);
    comps.push_back(std::move(comp));
    const Index modelId = layer.addModel("model", std::move(comps));
    REQUIRE(modelId >= 0);

    std::vector<Index> componentIds = layer.modelById(modelId)->componentIds();
    REQUIRE(!componentIds.empty());

    return componentIds;
}

static const MeshData* requireReadableMeshModel(const ModelPayload& payload)
{
    const auto& components = payload.components;
    REQUIRE(!components.empty());

    for (const auto& component : components) {
        if (component && component->mesh) {
            return component->mesh.get();
        }
    }

    FAIL("read model does not contain MeshData component");
    return nullptr;
}
} // namespace

TEST_CASE("PlyModelHandler ReadWrite simple.ply")
{
    PlyModelHandler handler;
    fs::path dir = test_dir_from_source();
    fs::path infile = dir / "simple.ply";
    REQUIRE(fs::exists(infile));

    std::optional<ModelPayload> payload = handler.read_model(infile, {});
    REQUIRE(payload.has_value());
    const MeshData* mesh = requireReadableMeshModel(*payload);
    REQUIRE(mesh != nullptr);

    REQUIRE(mesh->vertex_positions_.size() == 5u);
    REQUIRE(mesh->face_vertices_offset_.size() == 4u);
    REQUIRE(mesh->face_vertices_.size() == 3u * 3u);

    fs::path out = core::TempFile::instance().path().string() + "_simple_write.ply";

    MeshData mesh_copy;
    mesh_copy.init();
    mesh_copy.vertex_positions_ = mesh->vertex_positions_;
    mesh_copy.face_vertices_ = mesh->face_vertices_;
    mesh_copy.face_vertices_offset_ = mesh->face_vertices_offset_;

    ModelLayer layer;
    std::vector<Index> componentIds = addMeshModelAndGetComponentIds(
        layer,
        std::make_unique<MeshData>(std::move(mesh_copy)));

    REQUIRE_NOTHROW(handler.write_components(layer, componentIds, out, {}));
    REQUIRE(fs::exists(out));

    std::optional<ModelPayload> payload2 = handler.read_model(out, {});
    REQUIRE(payload2.has_value());
    const MeshData* mesh2 = requireReadableMeshModel(*payload2);
    REQUIRE(mesh2 != nullptr);

    REQUIRE(meshes_equal(*mesh, *mesh2));
}

TEST_CASE("PlyModelHandler ReadWrite test.ply")
{
    PlyModelHandler handler;
    fs::path dir = test_dir_from_source();
    fs::path infile = dir / "test.ply";
    REQUIRE(fs::exists(infile));

    std::optional<ModelPayload> payload = handler.read_model(infile, {});
    REQUIRE(payload.has_value());
    const MeshData* mesh = requireReadableMeshModel(*payload);
    REQUIRE(mesh != nullptr);

    REQUIRE(mesh->vertex_positions_.size() == 10u);
    REQUIRE(mesh->face_vertices_offset_.size() == 8u);

    fs::path out = core::TempFile::instance().path().string() + "_test_write.ply";

    MeshData mesh_copy;
    mesh_copy.init();
    mesh_copy.vertex_positions_ = mesh->vertex_positions_;
    mesh_copy.face_vertices_ = mesh->face_vertices_;
    mesh_copy.face_vertices_offset_ = mesh->face_vertices_offset_;

    ModelLayer layer;
    std::vector<Index> componentIds = addMeshModelAndGetComponentIds(
        layer,
        std::make_unique<MeshData>(std::move(mesh_copy)));

    REQUIRE_NOTHROW(handler.write_components(layer, componentIds, out, {}));
    REQUIRE(fs::exists(out));

    std::optional<ModelPayload> payload2 = handler.read_model(out, {});
    REQUIRE(payload2.has_value());
    const MeshData* mesh2 = requireReadableMeshModel(*payload2);
    REQUIRE(mesh2 != nullptr);

    REQUIRE(meshes_equal(*mesh, *mesh2));
}
