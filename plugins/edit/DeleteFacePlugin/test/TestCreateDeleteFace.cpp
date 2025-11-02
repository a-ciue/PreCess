#include "ArgObject.h"
#include "CreateFaceHandler.h"
#include "DeleteFaceHandler.h"
#include "MakeMeshData.h"
#include "ModelData.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("DeleteFace and CreateFace, Delete -> Create(Recover) -> Delete")
{
    using std::move;
    auto mesh_data_p = std::make_unique<MeshData>(MakeMeshData());
    auto mesh_data = mesh_data_p.get();
    ModelData model_data { move(mesh_data_p) };

    systems::edit::DeleteFaceHandler del_face;
    systems::edit::CreateFaceHandler create_face;

    size_t face_to_delete_at {};
    SECTION("try last face")
    {
        face_to_delete_at = mesh_data->face_vertices_offset_.size() - 2;
    }
    SECTION("try first face")
    {
        face_to_delete_at = 0;
    }

    auto vertices_to_delete_begin = mesh_data->face_vertices_.begin() + mesh_data->face_vertices_offset_[face_to_delete_at];
    auto vertices_to_delete_end = mesh_data->face_vertices_.begin() + mesh_data->face_vertices_offset_[face_to_delete_at + 1];
    std::vector vertices_to_delete(vertices_to_delete_begin, vertices_to_delete_end);

    auto first_del_selection = std::make_shared<Selection>(Selection { std::vector<Index> { static_cast<Index>(face_to_delete_at) }, ElementEnum::Face, 0 });
    auto end_del_selection = std::make_shared<Selection>(Selection { std::vector<Index> { (Index)mesh_data->face_vertices_offset_.size() - 2 }, ElementEnum::Face, 0 });
    core::ArgObject first_face_to_del = core::ArgObject::create<ArgTypeEnum::Selector>(first_del_selection);
    core::ArgObject end_face_to_del = core::ArgObject::create<ArgTypeEnum::Selector>(end_del_selection);
    core::ArgObject face_to_create = core::ArgObject::create<ArgTypeEnum::Selector>(std::make_shared<Selection>(Selection { vertices_to_delete, ElementEnum::Vertex, 0 }));

    ModelData del_model = del_face.execute(move(model_data), { first_face_to_del });
    ModelData recover_model = create_face.execute(move(del_model), { face_to_create });
    ModelData del2_model = del_face.execute(move(recover_model), { end_face_to_del });
    ModelData recover2_model = create_face.execute(move(del2_model), { face_to_create });

    MeshData* mesh_final = recover2_model.asMeshData();
    MeshData rhs = MakeMeshData();
    REQUIRE(mesh_final->vertex_positions_.size() == rhs.vertex_positions_.size());
    REQUIRE(mesh_final->face_vertices_.size() == rhs.face_vertices_.size());
    REQUIRE(mesh_final->face_vertices_offset_.size() == rhs.face_vertices_offset_.size());
    REQUIRE(mesh_final->edge_vertices_.size() == rhs.edge_vertices_.size());
    REQUIRE(mesh_final->solid_types_.size() == rhs.solid_types_.size());
    REQUIRE(mesh_final->solid_vertices_.size() == rhs.solid_vertices_.size());
    REQUIRE(mesh_final->solid_vertices_offset_.size() == rhs.solid_vertices_offset_.size());
    REQUIRE(mesh_final->solid_faces_vertices_.size() == rhs.solid_faces_vertices_.size());
    REQUIRE(mesh_final->solid_faces_vertices_offset_.size() == rhs.solid_faces_vertices_offset_.size());
    REQUIRE(mesh_final->solid_faces_.size() == rhs.solid_faces_.size());
    REQUIRE(mesh_final->solid_faces_offset_.size() == rhs.solid_faces_offset_.size());
}
