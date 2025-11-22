#include "ArgObject.h"
#include "CreateFaceHandler.h"
#include "MakeMeshData.h"
#include "ModelData.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("CreateFace with a variant of faces")
{
    using std::move;
    auto mesh_data = std::make_unique<MeshData>(MakeMeshData());
    ModelData model_data { move(mesh_data) };

    systems::edit::CreateFaceHandler create_face;

    SECTION("try a triangle")
    {
        auto selection = std::make_shared<Selection>(Selection { std::vector<Index> { 0, 1, 2 }, ElementEnum::Face, 0 });
        core::ArgObject faces = core::ArgObject::create<ArgTypeEnum::Selector>(selection);
        ModelData del_model = create_face.execute(move(model_data), { faces });
    }

    SECTION("try a quad")
    {
        auto selection = std::make_shared<Selection>(Selection { std::vector<Index> { 0, 1, 2, 3 }, ElementEnum::Face, 0 });
        core::ArgObject faces = core::ArgObject::create<ArgTypeEnum::Selector>(selection);
        ModelData del_model = create_face.execute(move(model_data), { faces });
    }
}
