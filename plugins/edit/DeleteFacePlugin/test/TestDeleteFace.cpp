#include "ArgObject.h"
#include "DeleteFaceHandler.h"
#include "MakeMeshData.h"
#include "ModelData.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("DeleteFace")
{
    using std::move;
    auto mesh_data = std::make_unique<MeshData>(MakeMeshData());
    ModelData model_data { move(mesh_data) };

    systems::edit::DeleteFaceHandler del_face;
    auto selection = std::make_shared<Selection>(Selection { std::vector<Index> { 0 }, ElementEnum::Face, 0 });
    core::ArgObject faces = core::ArgObject::create<ArgTypeEnum::Selector>(selection);
    ModelData del_model = del_face.execute(move(model_data), { faces });
}
