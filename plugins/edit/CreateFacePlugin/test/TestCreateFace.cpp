#include "ArgObject.h"
#include "CreateFaceHandler.h"
#include "MakeMeshData.h"
#include "MeshData.h"
#include "ModelData.h"
#include "ModelLayer.h"
#include "Selection.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("CreateFaceHandler: create triangle/quad on component mesh")
{
    using namespace systems::edit;

    auto build_one_component_model = []() -> std::pair<ModelLayer, Index> {
        auto mesh_data = std::make_unique<MeshData>(MakeMeshData());
        mesh_data->init();

        auto model = std::make_unique<ModelData>(std::move(mesh_data));

        ModelLayer mgr;
        const Index model_id = mgr.addModel(std::move(model));

        return { std::move(mgr), model_id };
    };

    SECTION("try a triangle")
    {
        auto [mgr, model_id] = build_one_component_model();

        auto cids = mgr.getComponentIds(model_id);
        REQUIRE(cids.size() == 1);

        auto comp_op_opt = mgr.getComponentOperator(cids[0]);
        REQUIRE(comp_op_opt.has_value());
        auto comp_op = std::move(*comp_op_opt);

        MeshData* mesh = comp_op.mesh();
        REQUIRE(mesh != nullptr);

        const Index base = mesh->global_point_base_;
        REQUIRE(base >= 0);

    
        auto selection = std::make_shared<Selection>(
            Selection { std::vector<Index> { base + 0, base + 1, base + 2 }, ElementEnum::Vertex, 0 });

        core::ArgObject arg0 = core::ArgObject::create<ArgTypeEnum::Selector>(selection);

        const Index old_face_vert_size = (Index)mesh->face_vertices_.size();
        const Index old_face_count = mesh->face_vertices_offset_.size() >= 1
            ? (Index)mesh->face_vertices_offset_.size() - 1
            : 0;

        CreateFaceHandler h;
        REQUIRE_NOTHROW(h.execute(comp_op, { arg0 }));

        REQUIRE((Index)mesh->face_vertices_.size() == old_face_vert_size + 3);
        REQUIRE((Index)mesh->face_vertices_offset_.size() == old_face_count + 2); // faces+1 => offsets size = faces+1
        REQUIRE(mesh->face_vertices_offset_.back() == (Index)mesh->face_vertices_.size());
    }

    SECTION("try a quad (4 vertices)")
    {
        auto [mgr, model_id] = build_one_component_model();

        auto cids = mgr.getComponentIds(model_id);
        REQUIRE(cids.size() == 1);

        auto comp_op_opt = mgr.getComponentOperator(cids[0]);
        REQUIRE(comp_op_opt.has_value());
        auto comp_op = std::move(*comp_op_opt);

        MeshData* mesh = comp_op.mesh();
        REQUIRE(mesh != nullptr);

        const Index base = mesh->global_point_base_;
        REQUIRE(base >= 0);

        auto selection = std::make_shared<Selection>(
            Selection { std::vector<Index> { base + 0, base + 1, base + 2, base + 3 }, ElementEnum::Vertex, 0 });

        core::ArgObject arg0 = core::ArgObject::create<ArgTypeEnum::Selector>(selection);

        const Index old_face_vert_size = (Index)mesh->face_vertices_.size();
        const Index old_face_count = mesh->face_vertices_offset_.size() >= 1
            ? (Index)mesh->face_vertices_offset_.size() - 1
            : 0;

        CreateFaceHandler h;
        REQUIRE_NOTHROW(h.execute(comp_op, { arg0 }));

        REQUIRE((Index)mesh->face_vertices_.size() == old_face_vert_size + 4);
        REQUIRE((Index)mesh->face_vertices_offset_.size() == old_face_count + 2);
        REQUIRE(mesh->face_vertices_offset_.back() == (Index)mesh->face_vertices_.size());
    }
}
