#include "ArgObject.h"
#include "CreateFaceHandler.h"
#include "DeleteFaceHandler.h"
#include "MakeMeshData.h"
#include "MeshData.h"
#include "ModelData.h"
#include "ModelManager.h"
#include "Selection.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("DeleteFace and CreateFace, Delete -> Create(Recover) -> Delete -> Create")
{
    using namespace systems::edit;

    MeshData baseline = MakeMeshData();

    auto mesh_data_p = std::make_unique<MeshData>(MakeMeshData());
    auto model = std::make_unique<ModelData>(std::move(mesh_data_p));

    ModelManager mgr;
    Index model_id = mgr.addModel(std::move(model));

    auto cids = mgr.getComponentIds(model_id);
    REQUIRE(cids.size() == 1);

    auto op_opt = mgr.getComponentOperator(cids[0]);
    REQUIRE(op_opt.has_value());
    auto op = std::move(*op_opt);

    MeshData* mesh = op.mesh();
    REQUIRE(mesh != nullptr);
    REQUIRE(mesh->face_vertices_offset_.size() >= 2);

    DeleteFaceHandler del_face;
    CreateFaceHandler create_face;

    Index face_to_delete_at = 0;

    SECTION("try last face")
    {
        face_to_delete_at = (Index)mesh->face_vertices_offset_.size() - 2;
    }
    SECTION("try first face")
    {
        face_to_delete_at = 0;
    }

    REQUIRE(face_to_delete_at >= 0);
    REQUIRE(face_to_delete_at < (Index)mesh->face_vertices_offset_.size() - 1);

    auto v_begin = mesh->face_vertices_.begin() + mesh->face_vertices_offset_[(size_t)face_to_delete_at];
    auto v_end = mesh->face_vertices_.begin() + mesh->face_vertices_offset_[(size_t)face_to_delete_at + 1];
    std::vector<Index> vertices_to_recover(v_begin, v_end);
    REQUIRE(vertices_to_recover.size() >= 3);

    auto sel_del_1 = std::make_shared<Selection>(
        Selection { std::vector<Index> { face_to_delete_at }, ElementEnum::Face, 0 });
    core::ArgObject arg_del_1 = core::ArgObject::create<ArgTypeEnum::Selector>(sel_del_1);
    REQUIRE_NOTHROW(del_face.execute(op, { arg_del_1 }));

    auto sel_create = std::make_shared<Selection>(
        Selection { vertices_to_recover, ElementEnum::Vertex, 0 });
    core::ArgObject arg_create = core::ArgObject::create<ArgTypeEnum::Selector>(sel_create);
    REQUIRE_NOTHROW(create_face.execute(op, { arg_create }));

    REQUIRE(mesh->face_vertices_offset_.size() >= 2);
    Index last_face_now = (Index)mesh->face_vertices_offset_.size() - 2;

    auto sel_del_2 = std::make_shared<Selection>(
        Selection { std::vector<Index> { last_face_now }, ElementEnum::Face, 0 });
    core::ArgObject arg_del_2 = core::ArgObject::create<ArgTypeEnum::Selector>(sel_del_2);
    REQUIRE_NOTHROW(del_face.execute(op, { arg_del_2 }));

    REQUIRE_NOTHROW(create_face.execute(op, { arg_create }));

    REQUIRE(mesh->vertex_count_ == (Index)baseline.vertex_positions_.size());

    REQUIRE(mesh->face_vertices_.size() == baseline.face_vertices_.size());
    REQUIRE(mesh->face_vertices_offset_.size() == baseline.face_vertices_offset_.size());
    REQUIRE(mesh->edge_vertices_.size() == baseline.edge_vertices_.size());
    REQUIRE(mesh->solid_types_.size() == baseline.solid_types_.size());
    REQUIRE(mesh->solid_vertices_.size() == baseline.solid_vertices_.size());
    REQUIRE(mesh->solid_vertices_offset_.size() == baseline.solid_vertices_offset_.size());
    REQUIRE(mesh->solid_faces_vertices_.size() == baseline.solid_faces_vertices_.size());
    REQUIRE(mesh->solid_faces_vertices_offset_.size() == baseline.solid_faces_vertices_offset_.size());
    REQUIRE(mesh->solid_faces_.size() == baseline.solid_faces_.size());
    REQUIRE(mesh->solid_faces_offset_.size() == baseline.solid_faces_offset_.size());
}
