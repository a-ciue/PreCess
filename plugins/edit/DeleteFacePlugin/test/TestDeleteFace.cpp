#include "ArgObject.h"
#include "DeleteFaceHandler.h"
#include "MakeMeshData.h"
#include "MeshData.h"
#include "ModelData.h"
#include "ModelManager.h"
#include "Selection.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("DeleteFaceHandler: delete face 0")
{
    using namespace systems::edit;

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

    const Index old_face_count = (Index)mesh->face_vertices_offset_.size() - 1;

    DeleteFaceHandler del_face;
    auto selection = std::make_shared<Selection>(
        Selection { std::vector<Index> { 0 }, ElementEnum::Face, 0 });
    core::ArgObject arg = core::ArgObject::create<ArgTypeEnum::Selector>(selection);

    REQUIRE_NOTHROW(del_face.execute(op, { arg }));

    const Index new_face_count = (Index)mesh->face_vertices_offset_.size() - 1;
    REQUIRE(new_face_count == old_face_count - 1);
}
