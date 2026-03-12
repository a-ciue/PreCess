// Test for PlyModelHandler: read PLY files, verify mesh content, write out and verify written file
#include "PlyModelHandler.h"
#include "ModelData.h"
#include "MeshData.h"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <string>
#include <cmath>

using namespace systems::io;
namespace fs = std::filesystem;

static bool models_equal(const ModelData& a, const ModelData& b, double eps = 1e-6)
{
    const auto* ma = a.asMeshData();
    const auto* mb = b.asMeshData();
    if (!ma || !mb) return false;

    if (ma->vertex_positions_.size() != mb->vertex_positions_.size()) return false;
    if (ma->face_vertices_offset_.size() != mb->face_vertices_offset_.size()) return false;
    if (ma->face_vertices_.size() != mb->face_vertices_.size()) return false;

    // compare vertices
    for (size_t i = 0; i < ma->vertex_positions_.size(); ++i) {
        for (int k = 0; k < 3; ++k) {
            double va = ma->vertex_positions_[i][k];
            double vb = mb->vertex_positions_[i][k];
            if (std::fabs(va - vb) > eps) return false;
        }
    }

    // compare face offsets
    for (size_t i = 0; i < ma->face_vertices_offset_.size(); ++i) {
        if (ma->face_vertices_offset_[i] != mb->face_vertices_offset_[i]) return false;
    }

    // compare face indices
    for (size_t i = 0; i < ma->face_vertices_.size(); ++i) {
        if (ma->face_vertices_[i] != mb->face_vertices_[i]) return false;
    }

    return true;
}

static fs::path test_dir_from_source()
{
    // Determine path to test files relative to this source file.
    fs::path p = fs::path(__FILE__).parent_path();
    return p;
}

TEST_CASE("PlyModelHandler ReadWrite simple.ply")
{
    PlyModelHandler handler;
    fs::path dir = test_dir_from_source();
    fs::path infile = dir / "simple.ply";
    REQUIRE(fs::exists(infile));

    auto model = handler.read_model(infile, {});
    REQUIRE(model != nullptr);
    auto* mesh = model->asMeshData();
    REQUIRE(mesh != nullptr);

    // simple.ply: 5 vertices, 3 faces
    REQUIRE(mesh->vertex_positions_.size() == 5u);
    REQUIRE(mesh->face_vertices_offset_.size() == 4u); // offsets contain 0 + 3 faces
    REQUIRE(mesh->face_vertices_.size() == 3u * 3u); // each face has 3 vertices in this file

    // write out
    fs::path out = std::filesystem::current_path() / "simple_write.ply";
    handler.write_model(*model, out, {});
    REQUIRE(fs::exists(out));

    // read back written file and compare semantically (positions and topology)
    auto model2 = handler.read_model(out, {});
    REQUIRE(model2 != nullptr);
    REQUIRE(models_equal(*model, *model2));
}

TEST_CASE("PlyModelHandler ReadWrite test.ply")
{
    PlyModelHandler handler;
    fs::path dir = test_dir_from_source();
    fs::path infile = dir / "test.ply";
    REQUIRE(fs::exists(infile));

    auto model = handler.read_model(infile, {});
    REQUIRE(model != nullptr);
    auto* mesh = model->asMeshData();
    REQUIRE(mesh != nullptr);

    // test.ply: 10 vertices, 7 faces
    REQUIRE(mesh->vertex_positions_.size() == 10u);
    REQUIRE(mesh->face_vertices_offset_.size() == 8u); // 0 + 7 faces

    // write out
    fs::path out = std::filesystem::current_path() / "test_write.ply";
    handler.write_model(*model, out, {});
    REQUIRE(fs::exists(out));

    // read back written file and compare semantically
    auto model2 = handler.read_model(out, {});
    REQUIRE(model2 != nullptr);
    REQUIRE(models_equal(*model, *model2));
}

