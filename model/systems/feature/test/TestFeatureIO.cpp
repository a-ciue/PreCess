/** @file TestFeatureIO.cpp
 * @brief IO 适配的路由、固定参数、导入记账、组件导出及注册清理回归。
 */
#include "ComponentData.h"
#include "FeatureContext.h"
#include "FeatureHandler.h"
#include "FeatureIOSystemRegister.h"
#include "FeatureRegistrar.h"
#include "FeatureSystem.h"
#include "HandlerCreatorDestroyerFactory.h"
#include "JobRunner.h"
#include "MeshData.h"
#include "ModelData.h"
#include "ModelIOHandler.h"
#include "ModelIOSystem.h"
#include "ModelLayer.h"
#include "ModelObserver.h"
#include "PluginBase.h"
#include "Selection.h"
#include "UndoStack.h"
#include "test/OwnerQueue.h"
#include <QJsonArray>
#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <stdexcept>
#include <thread>

using namespace systems::feature;
namespace {
class ImportModelObserver final : public ModelObserver {
public:
    Index last_added { -1 };
    std::vector<Index> added;
    void notifyModelAdded(Index id) override
    {
        last_added = id;
        added.push_back(id);
    }
    void notifyModelChanged(Index) override { }
    void notifyComponentChanged(Index) override { }
    void notifyModelRemoved(Index) override { }
    void notifyComponentRemoved(Index) override { }
    void notifyModelNameChanged(Index, const std::string&) override { }
    void notifyGeometryLoadFailed(const std::string&) override { }
};

class BridgeIOHandler final : public systems::io::ModelIOHandler {
public:
    inline static int next_id { 0 };
    inline static int last_reader { 0 };
    const int id { ++next_id };
    inline static std::size_t exported_points { 0 };
    inline static bool exported_option { false };
    inline static std::thread::id read_thread;
    inline static std::thread::id write_thread;
    inline static const ComponentData* exported_component { nullptr };
    std::optional<ModelPayload> read_model(const std::filesystem::path& path, const std::vector<std::any>& arguments) override
    {
        last_reader = id;
        read_thread = std::this_thread::get_id();
        if (path == "fail.dat")
            return std::nullopt;
        if (std::any_cast<long long>(arguments.at(0)) != 7)
            throw std::invalid_argument("Expected the Feature integer parameter");
        return ModelPayload { u8"桥接导入模型", { } };
    }
    void write_components(const ModelLayer& model, const std::vector<Index>& components, const std::filesystem::path&, const std::vector<std::any>& arguments) override
    {
        write_thread = std::this_thread::get_id();
        exported_component = model.findComponent(components.front());
        exported_points = model.findComponent(components.front())->mesh->vertex_positions_.size();
        exported_option = std::any_cast<bool>(arguments.at(0));
    }
    std::vector<core::ArgType> read_args_type() const override
    {
        return { { ArgTypeEnum::Int, "次数", "2", "" } };
    }
    std::vector<core::ArgType> write_args_type() const override { return { { ArgTypeEnum::Bool, "选项", "false", "" } }; }
};

class BridgeIOPlugin final : public systems::PluginBase {
    const systems::HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override
    {
        return systems::HandlerCreatorDestroyerFactory<BridgeIOHandler, systems::io::ModelIOHandler>::get();
    }
};

class FileFeatureHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& registrar, FeatureContext&) override
    {
        registrar.addParameter({ ArgTypeEnum::Path, "文件", "", "" });
    }
};
class NativeFileFeature final : public FeatureHandler {
public:
    explicit NativeFileFeature(FileTypeDeclaration declared)
        : declared_(std::move(declared))
    {
    }
    void setup(FeatureRegistrar& registrar, FeatureContext&) override
    {
        registrar.setFileType(declared_);
        registrar.addParameter({ ArgTypeEnum::Path, "文件", "", "" });
        if (declared_.direction == FileDirection::Export)
            registrar.addParameter({ ArgTypeEnum::Selector, "组件", "Component", "" });
        registrar.addParameter({ ArgTypeEnum::Bool, "Binary", "false", "" });
    }
    std::function<void(FeatureContext&)> action;
    std::any execute(FeatureContext& context) override
    {
        if (action)
            action(context);
        return { };
    }

private:
    FileTypeDeclaration declared_;
};
FeatureSystem::SystemHandlerPtr makeFileFeature()
{
    return FeatureSystem::SystemHandlerPtr { std::make_unique<FileFeatureHandler>().release() };
}
Index addTriangleComponent(ModelLayer& model, const std::string& name = "Target")
{
    auto component = std::make_unique<ComponentData>();
    component->name = name;
    component->mesh = std::make_unique<MeshData>();
    component->mesh->init();
    component->mesh->vertex_positions_ = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
    ComponentDatas components;
    components.push_back(std::move(component));
    const auto id = model.addModel(name, std::move(components));
    return model.modelById(id)->componentIds().front();
}
std::size_t pointCount(const ModelLayer& model, Index id)
{
    return model.findComponent(id)->mesh->vertex_positions_.size();
}
}
TEST_CASE("Legacy IO registration supplies a Feature import with parameters and undo", "[FeatureSystem][file][io]")
{
    core::EventBus bus;
    ImportModelObserver observer;
    ModelLayer model(&observer);
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    systems::io::ModelIOSystem io(model, &undo);
    FeatureSystem system(model, bus, &undo, &io);
    FeatureIOSystemRegister bridge(system);
    BridgeIOPlugin plugin;
    const QJsonObject metadata { { "file_type", "Bridge test format" }, { "extensions", QJsonArray { "dat" } } };
    int io_updates = 0;
    io.setOnDialogNameFiltersChanged([&] { ++io_updates; });
    REQUIRE(bridge.registerPlugin(metadata, plugin));
    const auto files = system.getFileTypes(FileDirection::Import);
    REQUIRE(files.size() == 1);
    const auto& feature = files.front().feature;
    REQUIRE(system.params(feature));
    REQUIRE(system.params(feature)->count() == 2);
    CHECK(system.params(feature)->types()[0].type == ArgTypeEnum::Paths);
    CHECK(system.params(feature)->types()[1].type == ArgTypeEnum::Int);
    REQUIRE(system.setParameter(feature, 0, core::ArgObject::create<ArgTypeEnum::Paths>({ "first.dat", "second.dat" })));
    REQUIRE(system.setParameter(feature, 1, core::ArgObject::create<ArgTypeEnum::Int>(7)));
    CHECK_FALSE(system.invoke(feature).has_value());
    REQUIRE(observer.added.size() == 2);
    const auto imported_model = observer.last_added;
    REQUIRE(imported_model >= 0);
    REQUIRE(model.modelById(imported_model));
    REQUIRE(undo.undo());
    CHECK_FALSE(model.modelById(observer.added.front()));
    CHECK_FALSE(model.modelById(imported_model));
    REQUIRE(undo.redo());
    CHECK(model.modelById(imported_model));
    REQUIRE(undo.undo());
    REQUIRE(undo.canRedo());
    REQUIRE(system.setParameter(feature, 0, core::ArgObject::create<ArgTypeEnum::Paths>({ "fail.dat" })));
    CHECK_FALSE(system.invoke(feature).has_value());
    CHECK_FALSE(model.modelById(imported_model));
    CHECK(undo.canRedo());
    CHECK_FALSE(bridge.registerPlugin(metadata, plugin));
    CHECK(io_updates == 1);
    auto operation = model.beginWriteOperation();
    REQUIRE(operation);
    CHECK_THROWS_AS(bridge.unregisterPlugin(metadata), ModelOperationBusy);
    CHECK(system.params(feature));
    REQUIRE(io.registeredFileTypeInfos().size() == 1);
    operation.reset();
    bridge.unregisterPlugin(metadata);
    CHECK(io_updates == 2);
    CHECK(system.getFileTypes(FileDirection::Import).empty());
    CHECK(io.registeredFileTypeInfos().empty());
}

TEST_CASE("Legacy file jobs preserve input and keep model ownership until completion", "[FeatureSystem][file][job]")
{
    OwnerQueue queue;
    core::EventBus bus;
    ImportModelObserver observer;
    ModelLayer model(&observer);
    const auto target = addTriangleComponent(model);
    observer.added.clear();
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    systems::io::ModelIOSystem io(model, &undo);
    systems::job::JobRunner runner(model, &undo, queue.dispatcher());
    FeatureSystem system(model, bus, &undo, &io);
    FeatureIOSystemRegister bridge(system);
    BridgeIOPlugin plugin;
    const QJsonObject metadata { { "file_type", "Bridge job" } };
    REQUIRE(bridge.registerPlugin(metadata, plugin));
    system.setJobRunner(&runner); // 注册后装配仍然生效。
    const auto importing = system.getFileTypes(FileDirection::Import).front().feature;
    REQUIRE(system.setParameter(importing, 0, core::ArgObject::create<ArgTypeEnum::Paths>({ "first.dat", "second.dat" })));
    REQUIRE(system.setParameter(importing, 1, core::ArgObject::create<ArgTypeEnum::Int>(7)));
    SECTION("Import parses off thread and installs the batch as one undo operation")
    {
        auto job = std::any_cast<std::shared_ptr<systems::job::Job>>(system.invoke(importing));
        REQUIRE(job);
        CHECK(job->owner() == importing);
        REQUIRE(system.setParameter(importing, 1, core::ArgObject::create<ArgTypeEnum::Int>(99)));
        auto finish = queue.take();
        CHECK(BridgeIOHandler::read_thread != std::this_thread::get_id());
        CHECK(model.writesPending());
        CHECK(observer.added.empty());
        CHECK_THROWS_AS(bridge.unregisterPlugin(metadata), ModelOperationBusy);
        CHECK_FALSE(undo.undo());
        finish();
        CHECK(job->state() == systems::job::JobState::Done);
        REQUIRE(observer.added.size() == 2);
        CHECK_FALSE(model.writesPending());
        REQUIRE(undo.undo());
        CHECK_FALSE(model.modelById(observer.added.front()));
        CHECK_FALSE(model.modelById(observer.added.back()));
        CHECK_FALSE(undo.canUndo());
    }
    SECTION("Export borrows the original component without copying or recording undo")
    {
        const auto exporting = system.getFileTypes(FileDirection::Export).front().feature;
        auto selected = std::make_shared<Selection>();
        selected->type = ElementEnum::Component;
        selected->ids = { target };
        REQUIRE(system.setParameter(exporting, 0, core::ArgObject::create<ArgTypeEnum::Path>("output.dat")));
        REQUIRE(system.setParameter(exporting, 1, core::ArgObject::create<ArgTypeEnum::Selector>(selected)));
        REQUIRE(system.setParameter(exporting, 2, core::ArgObject::create<ArgTypeEnum::Bool>(true)));
        auto job = std::any_cast<std::shared_ptr<systems::job::Job>>(system.invoke(exporting));
        REQUIRE(job);
        selected->ids.clear();
        auto finish = queue.take();
        CHECK(BridgeIOHandler::write_thread != std::this_thread::get_id());
        CHECK(BridgeIOHandler::exported_component == model.findComponent(target));
        CHECK(BridgeIOHandler::exported_option);
        CHECK(model.writesPending());
        CHECK_THROWS_AS(model.removeComponent(target), ModelOperationBusy);
        finish();
        CHECK(job->state() == systems::job::JobState::Done);
        CHECK_FALSE(undo.canUndo());
        CHECK_FALSE(model.writesPending());
    }
    SECTION("Cancellation discards parsed input without installing models")
    {
        auto job = std::any_cast<std::shared_ptr<systems::job::Job>>(system.invoke(importing));
        REQUIRE(job);
        job->cancel();
        queue.take()();
        CHECK(job->state() == systems::job::JobState::Cancelled);
        CHECK(observer.added.empty());
        CHECK_FALSE(undo.canUndo());
    }
    SECTION("Rejected publication does not fall back to synchronous IO")
    {
        runner.stop();
        BridgeIOHandler::read_thread = { };
        auto job = std::any_cast<std::shared_ptr<systems::job::Job>>(system.invoke(importing));
        CHECK_FALSE(job);
        CHECK(BridgeIOHandler::read_thread == std::thread::id { });
        CHECK(observer.added.empty());
    }
    runner.stop();
    system.setJobRunner(nullptr);
    bridge.unregisterPlugin(metadata);
}

TEST_CASE("Legacy IO bridge rejects conflicting Feature keys", "[FeatureSystem][file][io]")
{
    core::EventBus bus;
    ModelLayer model;
    systems::io::ModelIOSystem io(model);
    FeatureSystem system(model, bus, nullptr, &io);
    FeatureIOSystemRegister bridge(system);
    BridgeIOPlugin plugin;
    const QJsonObject metadata { { "file_type", "Bridge test format" }, { "extensions", QJsonArray { "dat" } } };
    HandlerMetaData reserved;
    reserved.name = "LegacyIO.Import/Bridge test format";
    REQUIRE(system.registerHandler(reserved, makeFileFeature()));
    CHECK_FALSE(bridge.registerPlugin(metadata, plugin));
    CHECK(io.registeredFileTypeInfos().empty());
    CHECK(system.params(reserved.name));
}

TEST_CASE("File Features sharing an extension keep independent IDs", "[FeatureSystem][file][io]")
{
    core::EventBus bus;
    ModelLayer model;
    systems::io::ModelIOSystem io(model);
    FeatureSystem system(model, bus, nullptr, &io);
    FeatureIOSystemRegister bridge(system);
    BridgeIOPlugin plugin;
    const QJsonObject a { { "file_type", "Format A" }, { "extensions", QJsonArray { "dat" } } };
    const QJsonObject b { { "file_type", "Format B" }, { "extensions", QJsonArray { "dat" } } };
    REQUIRE(bridge.registerPlugin(a, plugin));
    REQUIRE(bridge.registerPlugin(b, plugin));
    const auto formats = system.getFileTypes(FileDirection::Import);
    REQUIRE(formats.size() == 2);
    CHECK(formats[0].feature == "LegacyIO.Import/Format A");
    CHECK(formats[1].feature == "LegacyIO.Import/Format B");
    CHECK(formats[0].display_name == "Format A");
    CHECK(formats[0].extensions == formats[1].extensions);
    CHECK(formats[0].feature != formats[1].feature);
    CHECK(system.params("dat") == nullptr);
    int first_reader = 0;
    for (const auto& format : formats) {
        REQUIRE(system.setParameter(format.feature, 0, core::ArgObject::create<ArgTypeEnum::Paths>({ "input.dat" })));
        REQUIRE(system.setParameter(format.feature, 1, core::ArgObject::create<ArgTypeEnum::Int>(7)));
        REQUIRE_NOTHROW(system.invoke(format.feature));
        if (first_reader)
            CHECK(first_reader != BridgeIOHandler::last_reader);
        first_reader = BridgeIOHandler::last_reader;
    }
    bridge.unregisterPlugin(a);
    CHECK(system.params(formats[0].feature) == nullptr);
    REQUIRE(io.registeredFileTypeInfos().size() == 1);
    bridge.unregisterPlugin(b);
    CHECK(system.getFeatureInfos().empty());
}

TEST_CASE("Legacy export uses explicit components without adding undo history", "[FeatureSystem][file][io]")
{
    ModelLayer model;
    const auto target = addTriangleComponent(model);
    const auto other = addTriangleComponent(model, "Other");
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    core::EventBus bus;
    systems::io::ModelIOSystem io(model, &undo);
    FeatureSystem system(model, bus, &undo, &io);
    system.setActiveComponentProvider([other] { return std::optional<Index>(other); });
    FeatureIOSystemRegister bridge(system);
    BridgeIOPlugin plugin;
    REQUIRE(bridge.registerPlugin(QJsonObject { { "file_type", "Bridge test format" } }, plugin));
    const auto files = system.getFileTypes(FileDirection::Export);
    REQUIRE(files.size() == 1);
    const auto& feature = files.front().feature;
    REQUIRE(system.params(feature)->count() == 3);
    CHECK(system.params(feature)->types()[0].type == ArgTypeEnum::Path);
    CHECK(system.params(feature)->types()[1].type == ArgTypeEnum::Selector);
    REQUIRE(system.setParameter(feature, 0, core::ArgObject::create<ArgTypeEnum::Path>("output.dat")));
    CHECK(std::any_cast<std::string>(system.invoke(feature)).find("请选择") != std::string::npos);
    auto selected = std::make_shared<Selection>();
    selected->type = ElementEnum::Component;
    selected->ids = { target };
    REQUIRE(system.setParameter(feature, 1, core::ArgObject::create<ArgTypeEnum::Selector>(selected)));
    REQUIRE(system.setParameter(feature, 2, core::ArgObject::create<ArgTypeEnum::Bool>(true)));
    REQUIRE_NOTHROW(system.invoke(feature));
    CHECK(BridgeIOHandler::exported_points == 3);
    CHECK(BridgeIOHandler::exported_option);
    CHECK_FALSE(undo.scopeActive());
    CHECK(pointCount(model, target) == 3);
    CHECK(pointCount(model, other) == 3);
    selected->ids = { -1 };
    CHECK(std::any_cast<std::string>(system.invoke(feature)).find("失效") != std::string::npos);
    CHECK_FALSE(undo.scopeActive());
    CHECK(pointCount(model, target) == 3);
    CHECK_FALSE(undo.canUndo());
}

TEST_CASE("Native file features declare their parameters and work without IOSystem", "[FeatureSystem][file]")
{
    core::EventBus bus;
    ImportModelObserver observer;
    ModelLayer model(&observer);
    const auto target = addTriangleComponent(model);
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    FeatureSystem system(model, bus, &undo);
    REQUIRE(system.ioSystem() == nullptr);
    HandlerMetaData importing;
    importing.name = "NativeImport";
    auto reader = std::make_unique<NativeFileFeature>(FileTypeDeclaration { FileDirection::Import, { "dat" } });
    reader->action = [&](FeatureContext& context) {
        CHECK(*context.params.value(0).get<ArgTypeEnum::Path>() == "input.dat");
        CHECK(*context.params.value(1).get<ArgTypeEnum::Bool>());
        context.model.addModel("native import", { });
    };
    REQUIRE(system.registerHandler(importing, FeatureSystem::SystemHandlerPtr { reader.release() }));
    REQUIRE(system.params(importing.name)->count() == 2);
    CHECK(system.params(importing.name)->types()[0].type == ArgTypeEnum::Path);
    REQUIRE(system.setParameter(importing.name, 0, core::ArgObject::create<ArgTypeEnum::Path>("input.dat")));
    REQUIRE(system.setParameter(importing.name, 1, core::ArgObject::create<ArgTypeEnum::Bool>(true)));
    REQUIRE(system.getFileTypes(FileDirection::Import).front().feature == importing.name);
    system.invoke(importing.name);
    const auto added = observer.last_added;
    REQUIRE(model.modelById(added));
    REQUIRE(undo.undo());
    CHECK_FALSE(model.modelById(added));
    REQUIRE(undo.canRedo());

    HandlerMetaData exporting;
    exporting.name = "NativeExport";
    auto writer = std::make_unique<NativeFileFeature>(FileTypeDeclaration { FileDirection::Export, { "dat" } });
    bool called = false;
    writer->action = [&](FeatureContext& context) {
        called = true;
        CHECK(*context.params.value(0).get<ArgTypeEnum::Path>() == "output.dat");
        CHECK((*context.params.value(1).get<ArgTypeEnum::Selector>())->ids == std::vector<Index> { target });
        CHECK(*context.params.value(2).get<ArgTypeEnum::Bool>());
        CHECK(pointCount(context.model, target) == 3);
    };
    REQUIRE(system.registerHandler(exporting, FeatureSystem::SystemHandlerPtr { writer.release() }));
    REQUIRE(system.params(exporting.name)->count() == 3);
    CHECK(system.params(exporting.name)->types()[1].type == ArgTypeEnum::Selector);
    CHECK(system.params(exporting.name)->types()[1].content == "Component");
    auto selected = std::make_shared<Selection>();
    selected->type = ElementEnum::Component;
    selected->ids = { target };
    REQUIRE(system.setParameter(exporting.name, 0, core::ArgObject::create<ArgTypeEnum::Path>("output.dat")));
    REQUIRE(system.setParameter(exporting.name, 1, core::ArgObject::create<ArgTypeEnum::Selector>(selected)));
    REQUIRE(system.setParameter(exporting.name, 2, core::ArgObject::create<ArgTypeEnum::Bool>(true)));
    const auto label = undo.undoLabel();
    const auto redo_label = undo.redoLabel();
    REQUIRE(system.getFileTypes(FileDirection::Export).front().feature == exporting.name);
    system.invoke(exporting.name);
    CHECK(called);
    CHECK_FALSE(undo.scopeActive());
    CHECK(undo.undoLabel() == label);
    CHECK(undo.redoLabel() == redo_label);
    CHECK(pointCount(model, target) == 3);
    CHECK(undo.canRedo());
    system.unregisterHandler(importing);
    CHECK(system.getFileTypes(FileDirection::Import).empty());
    CHECK(system.getFileTypes(FileDirection::Export).size() == 1);
    system.unregisterHandler(exporting);
    CHECK(system.getFileTypes(FileDirection::Export).empty());
}

TEST_CASE("File Features use their own IDs while sharing labels and extensions", "[FeatureSystem][file]")
{
    core::EventBus bus;
    ModelLayer model;
    FeatureSystem system(model, bus);
    const auto add = [&](const std::string& name) {
        HandlerMetaData metadata;
        metadata.name = name;
        metadata.display_name = "Same format";
        auto handler = std::make_unique<NativeFileFeature>(FileTypeDeclaration { FileDirection::Import, { "dat" } });
        return system.registerHandler(metadata, FeatureSystem::SystemHandlerPtr { handler.release() });
    };
    REQUIRE(add("A"));
    REQUIRE(add("B"));
    REQUIRE(add("A"));
    const auto files = system.getFileTypes(FileDirection::Import);
    REQUIRE(files.size() == 2);
    CHECK(files[0].feature == "A");
    CHECK(files[1].feature == "B");
    CHECK(files[0].display_name == files[1].display_name);
    CHECK(files[0].extensions == files[1].extensions);
    CHECK(system.params("dat") == nullptr);

    systems::io::ModelIOSystem io(model);
    FeatureSystem mixed(model, bus, nullptr, &io);
    FeatureIOSystemRegister bridge(mixed);
    BridgeIOPlugin plugin;
    HandlerMetaData native;
    native.name = "Native A";
    native.display_name = "Format A";
    REQUIRE(mixed.registerHandler(native, FeatureSystem::SystemHandlerPtr { std::make_unique<NativeFileFeature>(FileTypeDeclaration { FileDirection::Import, { "dat" } }).release() }));
    REQUIRE(bridge.registerPlugin(QJsonObject { { "file_type", "Format A" }, { "extensions", QJsonArray { "dat" } } }, plugin));
    REQUIRE(bridge.registerPlugin(QJsonObject { { "file_type", "Legacy B" }, { "extensions", QJsonArray { "dat" } } }, plugin));
    CHECK(io.registeredFileTypeInfos().size() == 2);
    CHECK(mixed.getFileTypes(FileDirection::Import).size() == 3);
}
