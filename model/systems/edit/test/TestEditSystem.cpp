#include "ArgObject.h"
#include "ComponentData.h"
#include "EditSystem.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "ModelObserver.h"
#include "TrivialEditHandler.h"
#include "UndoStack.h"

#include <catch2/catch_test_macros.hpp>
#include <exception>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>

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
    std::any execute(ModelLayer& model, Index fallback_component_id, const std::vector<core::ArgObject>& /*args*/) override
    {
        auto op = model.getComponentOperator(fallback_component_id);
        if (op)
            op->appendPoint({ 2.0, 0.0, 0.0 });
        return { };
    }

    std::vector<core::ArgType> args_type() const override
    {
        return { };
    }
};

//! @brief 区分执行身份，并观察拒绝是否发生在元数据访问之前。
class RegistrationEditHandler : public EditHandler {
public:
    explicit RegistrationEditHandler(std::string value, int* metadata_calls = nullptr)
        : value(std::move(value))
        , metadata_calls(metadata_calls)
    {
    }
    std::any execute(ModelLayer& model, Index target, const std::vector<core::ArgObject>&) override
    {
        model.getComponentOperator(target)->appendPoint({ 9, 0, 0 });
        return value;
    }
    std::vector<core::ArgType> args_type() const override
    {
        if (metadata_calls)
            ++*metadata_calls;
        if (reject_metadata)
            throw std::runtime_error("edit metadata failed");
        return { { ArgTypeEnum::Text, value, { }, { } } };
    }
    std::string value;
    int* metadata_calls;
    bool reject_metadata { false };
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
    c->id = -1;
    c->name = "Comp_0";
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
    system.call("WritingEdit", component_id, { });
    REQUIRE(obs.component_changed_count == count_after_add + 1);
    REQUIRE(obs.last_component_changed == component_id);

    // handler 不写 → flush 空转，无通知（修正原无条件 notify 的过度通知）
    system.call("TrivialEdit", component_id, { });
    REQUIRE(obs.component_changed_count == count_after_add + 1);
}

TEST_CASE("Edit registration keeps stable information and replaces handler with metadata", "[EditSystem][registration]")
{
    ModelLayer model;
    const Index target = addTriangleComponent(model);
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    EditSystem system(model, &undo);
    HandlerMetaData meta { "Registered", "Original label" };
    auto first = std::make_unique<RegistrationEditHandler>("first");
    auto* original = first.get();
    REQUIRE(system.registerHandler(meta, EditSystem::SystemHandlerPtr { first.release() }));
    auto* info = system.getEditInfos().front();
    for (int i = 0; i < 256; ++i) {
        const auto name = "Other" + std::to_string(i);
        auto handler = std::make_unique<RegistrationEditHandler>(name);
        REQUIRE(system.registerHandler({ name, name }, EditSystem::SystemHandlerPtr { handler.release() }));
    }
    auto find_info = [&]() -> EditInfo* {
        for (auto* entry : system.getEditInfos())
            if (entry->name == meta.name)
                return entry;
        return nullptr;
    };
    REQUIRE(find_info() == info);
    original->value = "changed";
    REQUIRE(system.getArgTypes(meta.name)->front().name == "changed");
    REQUIRE(info->arg_types.front().name == "first");
    REQUIRE(std::any_cast<std::string>(system.call(meta.name, target, { })) == "changed");
    REQUIRE(undo.undoLabel() == "Original label");
    auto second = std::make_unique<RegistrationEditHandler>("second");
    meta.display_name.clear();
    REQUIRE(system.registerHandler(meta, EditSystem::SystemHandlerPtr { second.release() }));
    REQUIRE(system.getEditInfos().size() == 257);
    REQUIRE(find_info()->display_name.empty());
    REQUIRE(find_info()->arg_types.front().name == "second");
    REQUIRE(system.getArgTypes(meta.name)->front().name == "second");
    REQUIRE(std::any_cast<std::string>(system.call(meta.name, target, { })) == "second");
    REQUIRE(undo.undoLabel() == meta.name);
    REQUIRE(undo.undo());
    REQUIRE(undo.undoLabel() == "Original label");
    REQUIRE(undo.undo());
    REQUIRE(model.findComponent(target)->mesh->vertex_positions_.size() == 3);
    system.unregisterHandler(meta);
    REQUIRE_FALSE(find_info());
    REQUIRE_FALSE(system.getArgTypes(meta.name));
    REQUIRE(system.getEditInfos().size() == 256);
}

TEST_CASE("Edit metadata failure preserves the original registration", "[EditSystem][registration]")
{
    ModelLayer model;
    const Index target = addTriangleComponent(model);
    EditSystem system(model);
    auto original = std::make_unique<RegistrationEditHandler>("original");
    REQUIRE(system.registerHandler({ "Registered", "Original label" }, EditSystem::SystemHandlerPtr { original.release() }));
    const auto infos = system.getEditInfos();
    int notifications = 0;
    system.setOnEditInfoChangedCallback([&] { ++notifications; });
    HandlerMetaData meta { "Registered", "Rejected label" };
    SECTION("replacement") { }
    SECTION("new entry") { meta.name = "New"; }
    auto rejected = std::make_unique<RegistrationEditHandler>("rejected");
    rejected->reject_metadata = true;
    REQUIRE_THROWS_AS(system.registerHandler(meta, EditSystem::SystemHandlerPtr { rejected.release() }), std::runtime_error);
    REQUIRE_FALSE(system.registerHandler(meta, { }));
    REQUIRE(notifications == 0);
    REQUIRE(system.getEditInfos() == infos);
    REQUIRE(infos.front()->display_name == "Original label");
    REQUIRE(system.getArgTypes("Registered")->front().name == "original");
    REQUIRE(std::any_cast<std::string>(system.call("Registered", target, { })) == "original");
    REQUIRE_FALSE(system.getArgTypes("New"));
}

TEST_CASE("Edit registration changes require an idle owner thread even with write privilege", "[EditSystem][operation]")
{
    ModelLayer model;
    EditSystem system(model);
    const HandlerMetaData meta { "Registered", "Original" };
    auto original = std::make_unique<RegistrationEditHandler>("original");
    REQUIRE(system.registerHandler(meta, EditSystem::SystemHandlerPtr { original.release() }));
    const auto infos = system.getEditInfos();
    int notifications = 0;
    int metadata_calls = 0;
    system.setOnEditInfoChangedCallback([&] { ++notifications; });
    auto operation = model.beginWriteOperation();
    REQUIRE(operation);
    {
        ModelLayer::WritePrivilege privilege(model, operation.get());
        auto rejected = std::make_unique<RegistrationEditHandler>("rejected", &metadata_calls);
        REQUIRE_THROWS_AS(system.registerHandler(meta, EditSystem::SystemHandlerPtr { rejected.release() }), ModelOperationBusy);
        REQUIRE_THROWS_AS(system.unregisterHandler(meta), ModelOperationBusy);
    }
    REQUIRE(metadata_calls == 0);
    REQUIRE(notifications == 0);
    REQUIRE(system.getEditInfos() == infos);
    operation.reset();
    std::exception_ptr registration_error, removal_error;
    std::thread worker([&] {
        try {
            auto rejected = std::make_unique<RegistrationEditHandler>("worker", &metadata_calls);
            system.registerHandler(meta, EditSystem::SystemHandlerPtr { rejected.release() });
        } catch (...) {
            registration_error = std::current_exception();
        }
        try {
            system.unregisterHandler(meta);
        } catch (...) {
            removal_error = std::current_exception();
        }
    });
    worker.join();
    REQUIRE(registration_error);
    REQUIRE(removal_error);
    REQUIRE(metadata_calls == 0);
    REQUIRE(notifications == 0);
    REQUIRE(system.getEditInfos() == infos);
    auto replacement = std::make_unique<RegistrationEditHandler>("replacement", &metadata_calls);
    REQUIRE(system.registerHandler(meta, EditSystem::SystemHandlerPtr { replacement.release() }));
    REQUIRE(metadata_calls == 1);
    REQUIRE_NOTHROW(system.unregisterHandler(meta));
    REQUIRE(system.getEditInfos().empty());
}
