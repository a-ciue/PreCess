#include "LibMeshbIO.h"
#include "MakeMeshData.h"
#include "TempFile.h"
#include <catch2/catch_test_macros.hpp>
#include <libmeshb7.h>

TEST_CASE("LibMeshbIO::write()/read() over MakeMeshData(), with version 1")
{
    MeshData mesh_data = MakeMeshData();

    LibMeshbIO io;
    std::string out = core::TempFile::instance().path().string() + ".mesh";

    int64_t write_idx = GmfOpenMesh(out.c_str(), GmfWrite, 1, 3);
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

TEST_CASE("LibMeshbIO::write()/read() over MakeMeshData(), with version 3")
{
    MeshData mesh_data = MakeMeshData();

    LibMeshbIO io;
    std::string out = core::TempFile::instance().path().string() + ".mesh";
    int64_t write_idx = GmfOpenMesh(out.c_str(), GmfWrite, 3, 3);
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

    MeshData rhs;
    REQUIRE(io.read(out, rhs));

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