#include "ArgObject.h"
#include "ComponentData.h"
#include "ComponentOperator.h"
#include "CreateFaceHandler.h"
#include "MakeMeshData.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "Selection.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("CreateFaceHandler: create triangle/quad on component mesh")
{
    using namespace systems::edit;

    auto build_one_component_model = []() -> std::pair<ModelLayer, Index> {
        auto mesh_data = std::make_unique<MeshData>(MakeMeshData());

        auto c = std::make_unique<ComponentData>();
        c->id = -1; c->name = "Comp_0";
        c->mesh = std::move(mesh_data);
        ComponentDatas comps;
        comps.push_back(std::move(c));

        ModelLayer mgr;
        const Index model_id = mgr.addModel("test_model", std::move(comps));

        return { std::move(mgr), model_id };
    };

    SECTION("try a triangle")
    {
        auto [mgr, model_id] = build_one_component_model();

        auto cids = mgr.modelById(model_id)->componentIds();
        REQUIRE(cids.size() == 1);

        ComponentData* c = mgr.findComponent(cids[0]);
        REQUIRE(c);
        ComponentOperator comp_op(cids[0], *c, mgr);

        const MeshData* mesh = comp_op.mesh();
        REQUIRE(mesh != nullptr);

        // 选择 id 使用组件入池时分配的全局点 id（point_global_ids_）

    
        auto selection = std::make_shared<Selection>(
            Selection { std::vector<Index> { c->point_global_ids_[0], c->point_global_ids_[1],
                            c->point_global_ids_[2] },
                ElementEnum::Vertex, 0 });

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

        auto cids = mgr.modelById(model_id)->componentIds();
        REQUIRE(cids.size() == 1);

        ComponentData* c = mgr.findComponent(cids[0]);
        REQUIRE(c);
        ComponentOperator comp_op(cids[0], *c, mgr);

        const MeshData* mesh = comp_op.mesh();
        REQUIRE(mesh != nullptr);

        // 选择 id 使用组件入池时分配的全局点 id（point_global_ids_）

        auto selection = std::make_shared<Selection>(
            Selection { std::vector<Index> { c->point_global_ids_[0], c->point_global_ids_[1],
                            c->point_global_ids_[2], c->point_global_ids_[3] },
                ElementEnum::Vertex, 0 });

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
