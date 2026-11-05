#include "MakeMeshData.h"
#include "ComponentData.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "ModelPayload.h"
#include "TempFile.h"
#include "VtkLegacyModelHandler.h"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
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
    auto cids = mgr.modelById(model_id)->componentIds();
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

TEST_CASE("VTK legacy export preserves double coordinates and text precision", "[vtk][precision]")
{
    const std::vector<std::array<double, 3>> expected {
        { 1.00000001, 0, 0 }, { 1.00000002, 0, 0 }, { 170.6367188, 1, -0.12345678901234567 }
    };
    auto mesh = std::make_unique<MeshData>();
    mesh->init();
    mesh->vertex_positions_ = expected;
    mesh->face_vertices_ = { 0, 1, 2 };
    mesh->face_vertices_offset_ = { 0, 3 };
    auto component = std::make_unique<ComponentData>();
    component->mesh = std::move(mesh);
    ComponentDatas components;
    components.push_back(std::move(component));
    ModelLayer layer;
    const auto model_id = layer.addModel("precision", std::move(components));
    const std::filesystem::path path = core::TempFile::instance().path().string() + "_double.vtk";
    systems::io::VtkLegacyModelHandler handler;
    handler.write_components(layer, layer.modelById(model_id)->componentIds(), path, {});
    std::ifstream file(path);
    const std::string text((std::istreambuf_iterator<char>(file)), {});
    CHECK(text.find("POINTS 3 double") != std::string::npos);
    const auto payload = handler.read_model(path, {});
    REQUIRE(payload.has_value());
    REQUIRE(payload->components.size() == 1);
    const auto& result = *payload->components.front()->mesh;
    CHECK(result.vertex_positions_ == expected);
    CHECK(result.face_vertices_ == std::vector<Index> { 0, 1, 2 });
    CHECK(result.face_vertices_offset_ == std::vector<Index> { 0, 3 });
}
