#include "MakeMeshData.h"
#include "ComponentData.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "ModelPayload.h"
#include "TempFile.h"
#include "VtkLegacyModelHandler.h"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <memory>
#include <optional>

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
    ComponentDatas comps;
    auto comp = std::make_unique<ComponentData>();
    comp->id = -1;
    comp->mesh = std::move(mesh_data);
    comps.push_back(std::move(comp));

    ModelLayer mgr;
    Index model_id = mgr.addModel("model", std::move(comps));
    auto cids = mgr.getComponentIds(model_id);
    REQUIRE(!cids.empty());

    REQUIRE_NOTHROW(io.write_components(mgr, cids, out, {}));
    REQUIRE(std::filesystem::exists(out));

    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = io.read_model(out, {}));
    REQUIRE(payload.has_value());

    const auto& read_comps = payload->components;
    REQUIRE(!read_comps.empty());
    REQUIRE(read_comps[0]);
    REQUIRE(read_comps[0]->mesh);

    const MeshData* read_mesh = read_comps[0]->mesh.get();
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
