#include "MakeMeshData.h"
#include "MeshData.h"
#include "ModelData.h"
#include "ModelManager.h"
#include "TempFile.h"
#include "VtkLegacyModelHandler.h"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <memory>

TEST_CASE("VtkLegacyModelHandler::write_components()/read_model() over MakeMeshData()")
{
    systems::io::VtkLegacyModelHandler io;
    std::filesystem::path out;

    SECTION("write/read on path made by Latin letters")
    {
        out = core::TempFile::instance().path();
        out.replace_extension(".vtk");
    }
    SECTION("write/read on path with Chinese letters")
    {
        out = core::TempFile::instance().path();
        out.replace_filename("中文" + out.stem().string());
        out.replace_extension(".vtk");
    }

    MeshData rhs = MakeMeshData();

    auto mesh_data = std::make_unique<MeshData>(MakeMeshData());
    auto model = std::make_unique<ModelData>(std::move(mesh_data));

    ModelManager mgr;
    Index model_id = mgr.addModel(std::move(model));
    auto cids = mgr.getComponentIds(model_id);
    REQUIRE(!cids.empty());

    REQUIRE_NOTHROW(io.write_components(mgr, cids, out, {}));
    REQUIRE(std::filesystem::exists(out));

    std::unique_ptr<ModelData> read_model;
    REQUIRE_NOTHROW(read_model = io.read_model(out, {}));
    REQUIRE(read_model);

    const auto& comps = read_model->stagingcomponents();
    REQUIRE(!comps.empty());
    REQUIRE(comps[0]);
    REQUIRE(comps[0]->mesh);

    const MeshData* read_mesh = comps[0]->mesh.get();
    REQUIRE(read_mesh);

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
