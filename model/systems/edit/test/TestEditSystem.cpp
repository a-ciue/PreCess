#include "ArgObject.h"
#include "ComponentData.h"
#include "EditSystem.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "ModelObserver.h"
#include "TrivialEditHandler.h"

#include <catch2/catch_test_macros.hpp>

using namespace systems::edit;

namespace {
struct CountingObserver : ModelObserver {
    int component_changed_count { 0 };
    Index last_component_changed { -1 };

    void notifyModelChanged(Index) override { }
    void notifyModelAdded(Index) override { }
    void notifyModelRemoved(Index) override { }
    void notifyComponentRemoved(Index) override { }
    void notifyComponentChanged(Index component_id) override
    {
        ++component_changed_count;
        last_component_changed = component_id;
    }
    void notifyModelNameChanged(Index, const std::string&) override { }
    void notifyGeometryLoadFailed(const std::string&) override { }
};

//! @brief 写模型的编辑 handler：经可写入口写一次（写必脏，通知待操作边界 flush）
class WritingEditHandler : public EditHandler {
public:
    std::any execute(ComponentOperator& op, const std::vector<core::ArgObject>& /*args*/) override
    {
        op.appendPoint({ 2.0, 0.0, 0.0 });
        return {};
    }

    std::vector<core::ArgType> args_type() const override
    {
        return {};
    }
};

//! @brief 构造一个简单三角形面片组件并入池，返回 component_id
Index addTriangleComponent(ModelLayer& mgr)
{
    auto mesh = std::make_unique<MeshData>();
    mesh->init();
    mesh->vertex_positions_ = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
    mesh->face_vertices_ = { 0, 1, 2 };
    mesh->face_vertices_offset_ = { 0, 3 };

    auto c = std::make_unique<ComponentData>();
    c->name = "Comp_0";
    c->mesh = std::move(mesh);
    ComponentDatas comps;
    comps.push_back(std::move(c));

    const Index model_id = mgr.addModel("edit_boundary_test", std::move(comps));
    return mgr.modelById(model_id)->componentIds()[0];
}
}

TEST_CASE("EditSystem::register&unregister")
{
    auto mesh_data = std::make_unique<MeshData>();
    mesh_data->init();

    auto c = std::make_unique<ComponentData>();
    c->id = -1; c->name = "Comp_0";
    c->mesh = std::move(mesh_data);
    ComponentDatas comps;
    comps.push_back(std::move(c));

    ModelLayer model_manager;
    REQUIRE_NOTHROW(model_manager.addModel("test_model", std::move(comps)));
    EditSystem system(model_manager);

    EditSystem::SystemHandlerPtr handler { new TrivialEditHandler() };
    HandlerMetaData meta_data;
    meta_data.name = "TrivialEdit";
    meta_data.display_name = "Trivial Edit Handler";
    REQUIRE(system.registerHandler(meta_data, std::move(handler)));
    REQUIRE_NOTHROW(system.unregisterHandler(meta_data));
}

TEST_CASE("EditSystem::call flushes notifications at the operation boundary", "[EditSystem]")
{
    CountingObserver obs;
    ModelLayer model_manager(&obs);
    const Index component_id = addTriangleComponent(model_manager);
    const int count_after_add = obs.component_changed_count;
    EditSystem system(model_manager);

    HandlerMetaData write_meta;
    write_meta.name = "WritingEdit";
    write_meta.display_name = "Writing Edit Handler";
    EditSystem::SystemHandlerPtr writing { new WritingEditHandler() };
    REQUIRE(system.registerHandler(write_meta, std::move(writing)));

    HandlerMetaData trivial_meta;
    trivial_meta.name = "TrivialEdit";
    trivial_meta.display_name = "Trivial Edit Handler";
    EditSystem::SystemHandlerPtr trivial { new TrivialEditHandler() };
    REQUIRE(system.registerHandler(trivial_meta, std::move(trivial)));

    // handler 写一次 → 操作边界 flush 通知一次
    system.call("WritingEdit", component_id, {});
    REQUIRE(obs.component_changed_count == count_after_add + 1);
    REQUIRE(obs.last_component_changed == component_id);

    // handler 不写 → flush 空转，无通知（修正原无条件 notify 的过度通知）
    system.call("TrivialEdit", component_id, {});
    REQUIRE(obs.component_changed_count == count_after_add + 1);
}
