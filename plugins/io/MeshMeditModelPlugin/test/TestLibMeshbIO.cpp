#include "LibMeshbIO.h"
#include "MakeMeshData.h"
#include "TempFile.h"
#include <catch2/catch_test_macros.hpp>
#include <libmeshb7.h>

TEST_CASE("LibMeshbIO::write()/read() over MakeMeshData()")
{
    MeshData mesh_data = MakeMeshData();

    LibMeshbIO io;
    std::string out = core::TempFile::instance().path().string() + ".mesh";

    int64_t write_idx {};
    SECTION("with file version 1")
    {
        write_idx = GmfOpenMesh(out.c_str(), GmfWrite, 1, 3);
    }
    SECTION("with file version 2")
    {
        write_idx = GmfOpenMesh(out.c_str(), GmfWrite, 2, 3);
    }
    SECTION("with file version 3")
    {
        write_idx = GmfOpenMesh(out.c_str(), GmfWrite, 3, 3);
    }
    SECTION("with file version 4")
    {
        write_idx = GmfOpenMesh(out.c_str(), GmfWrite, 4, 3);
    }

    try {
        REQUIRE(write_idx != 0);
        REQUIRE(io.write(write_idx, mesh_data));
    } catch (...) {
        if (write_idx) {
            GmfCloseMesh(write_idx);
        }
        throw;
    }
    if (write_idx) {
        GmfCloseMesh(write_idx);
    }

    REQUIRE(io.read(out, mesh_data));

    MeshData rhs = MakeMeshData();
    REQUIRE(mesh_data.vertex_positions_.size() == rhs.vertex_positions_.size());
    REQUIRE(mesh_data.face_vertices_.size() == rhs.face_vertices_.size());
    REQUIRE(mesh_data.face_vertices_offset_.size() == rhs.face_vertices_offset_.size());
    REQUIRE(mesh_data.edge_vertices_.size() == rhs.edge_vertices_.size());
    REQUIRE(mesh_data.solid_types_.size() == rhs.solid_types_.size());
    REQUIRE(mesh_data.solid_vertices_.size() == rhs.solid_vertices_.size());
    REQUIRE(mesh_data.solid_vertices_offset_.size() == rhs.solid_vertices_offset_.size());
    REQUIRE(mesh_data.solid_faces_vertices_.size() == rhs.solid_faces_vertices_.size());
    REQUIRE(mesh_data.solid_faces_vertices_offset_.size() == rhs.solid_faces_vertices_offset_.size());
    REQUIRE(mesh_data.solid_faces_.size() == rhs.solid_faces_.size());
    REQUIRE(mesh_data.solid_faces_offset_.size() == rhs.solid_faces_offset_.size());
}

TEST_CASE("Medit preserves double exports while retaining version one compatibility", "[medit][precision]")
{
    const std::vector<std::array<double, 3>> expected {
        { 1.00000001, 0, 0 }, { 1.00000002, 0, 0 }, { 170.6367188, 1, -0.12345678901234567 }
    };
    for (const auto* extension : { ".mesh", ".meshb" }) {
        for (int version : { 1, 2, 3, 4 }) {
            DYNAMIC_SECTION("version " << version << " extension " << extension)
            {
                MeshData mesh;
                mesh.init();
                mesh.vertex_positions_ = expected;
                mesh.vertex_count_ = 3;
                mesh.face_vertices_ = { 0, 1, 2 };
                mesh.face_vertices_offset_ = { 0, 3 };
                const auto path = core::TempFile::instance().path().string() + "_precision" + extension;
                const auto handle = GmfOpenMesh(path.c_str(), GmfWrite, version, 3);
                REQUIRE(handle != 0);
                const bool written = LibMeshbIO::write(handle, mesh);
                const int closed = GmfCloseMesh(handle);
                REQUIRE(written);
                REQUIRE(closed != 0);
                MeshData result;
                REQUIRE(LibMeshbIO::read(path, result));
                auto reference = expected;
                // v1 是格式本身的单精度限制；v2/v3/v4 必须精确保留 double。
                if (version == 1)
                    for (auto& point : reference)
                        for (auto& coordinate : point)
                            coordinate = static_cast<double>(static_cast<float>(coordinate));
                CHECK(result.vertex_positions_ == reference);
                CHECK(result.face_vertices_ == mesh.face_vertices_);
                CHECK(result.face_vertices_offset_ == mesh.face_vertices_offset_);
            }
        }
    }
}
