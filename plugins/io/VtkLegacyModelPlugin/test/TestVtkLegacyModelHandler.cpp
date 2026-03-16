#include "MakeMeshData.h"
#include "ModelData.h"
#include "TempFile.h"
#include "VtkLegacyModelHandler.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("VtkLegacyModelHandler::write_model()/read_model() over MakeMeshData()")
{
    systems::io::VtkLegacyModelHandler io;
    std::filesystem::path out;

    SECTION("write/read on path made by Latin letters")
    {
        out = core::TempFile::instance().path().string() + ".vtk";
    }
    SECTION("write/read on path with Chinese letters")
    {
        out = core::TempFile::instance().path();
        out.replace_filename("中文" + out.stem().string() + ".vtk");
    }

    std::unique_ptr mesh_data = std::make_unique<MeshData>(MakeMeshData());
    std::unique_ptr<ModelData> model = std::make_unique<ModelData>(std::move(mesh_data));
    REQUIRE_NOTHROW(io.write_model(*model, out, {}));

    REQUIRE_NOTHROW(model = io.read_model(out, {}));
    REQUIRE(model);
    const MeshData* read_mesh = model->asMeshData();
    REQUIRE(read_mesh);

    MeshData rhs = MakeMeshData();
    REQUIRE(read_mesh->vertex_positions_.size() == rhs.vertex_positions_.size());
    REQUIRE(read_mesh->face_vertices_.size() == rhs.face_vertices_.size());
    REQUIRE(read_mesh->face_vertices_offset_.size() == rhs.face_vertices_offset_.size());
    REQUIRE(read_mesh->edge_vertices_.size() == rhs.edge_vertices_.size());
    REQUIRE(read_mesh->solid_types_.size() == rhs.solid_types_.size());
    REQUIRE(read_mesh->solid_vertices_.size() == rhs.solid_vertices_.size());
    REQUIRE(read_mesh->solid_vertices_offset_.size() == rhs.solid_vertices_offset_.size());
    REQUIRE(read_mesh->solid_faces_vertices_.size() == rhs.solid_faces_vertices_.size());
    REQUIRE(read_mesh->solid_faces_vertices_offset_.size() == rhs.solid_faces_vertices_offset_.size());
    REQUIRE(read_mesh->solid_faces_.size() == rhs.solid_faces_.size());
    REQUIRE(read_mesh->solid_faces_offset_.size() == rhs.solid_faces_offset_.size());
}
