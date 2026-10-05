#include "AlgorithmHandler.h"
#include "AlgorithmSystem.h"
#include "ArgObject.h"
#include "ComponentData.h"
#include "GeometryBuilder.h"
#include "GeometryData.h"
#include "JobRunner.h"
#include "MeshData.h"
#include "ModelData.h"
#include "ModelIOHandler.h"
#include "ModelIOSystem.h"
#include "ModelLayer.h"
#include "ModelObserver.h"
#include "ModelPayload.h"
#include "ModelScope.h"
#include "Selection.h"
#include "UndoStack.h"
#include "test/OwnerQueue.h"

#include <catch2/catch_test_macros.hpp>
#include <exception>
#include <filesystem>
#include <memory>
#include <semaphore>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

using namespace systems::algo;
using systems::io::ModelIOSystem;

namespace {
struct AsyncRun {
    OwnerQueue queue;
    systems::job::JobRunner runner;
    std::shared_ptr<systems::job::Job> job;
    std::function<void()> completion;
    AsyncRun(AlgorithmSystem& system, ModelLayer& model, UndoStack* stack, const std::string& name, Index target, ProgressFn progress = { })
        : runner(model, stack, queue.dispatcher())
    {
        system.setJobRunner(&runner);
        runner.setOnProgress([progress](systems::job::Job&, double value, const std::string& text) {
            if (progress)
                progress(value, text);
        });
        job = system.callAsync(name, target, { });
        REQUIRE(job);
        completion = queue.take();
    }
    void complete()
    {
        completion();
        REQUIRE(job->state() == systems::job::JobState::Done);
    }
};

//! @brief 计数观察者：验证提交后的模型通知与 undo 后的结构变化
struct CountingObserver : ModelObserver {
    int component_changed_count { 0 };
    int model_added_count { 0 };
    Index last_model_added { -1 };

    void notifyModelChanged(Index) override { }
    void notifyComponentChanged(Index) override { ++component_changed_count; }
    void notifyModelAdded(Index model_id) override
    {
        ++model_added_count;
        last_model_added = model_id;
    }
    void notifyModelRemoved(Index) override { }
    void notifyComponentRemoved(Index) override { }
    void notifyModelNameChanged(Index, const std::string&) override { }
    void notifyGeometryLoadFailed(const std::string&) override { }
};

//! @brief 构造一个简单三角形面片组件并入池，返回 component_id
Index addTriangleComponent(ModelLayer& mgr, const std::string& name = "Comp_0")
{
    auto mesh = std::make_unique<MeshData>();
    mesh->init();
    mesh->vertex_positions_ = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
    mesh->face_vertices_ = { 0, 1, 2 };
    mesh->face_vertices_offset_ = { 0, 3 };

    auto c = std::make_unique<ComponentData>();
    c->name = name;
    c->mesh = std::move(mesh);
    ComponentDatas comps;
    comps.push_back(std::move(c));

    const Index model_id = mgr.addModel(name + "_model", std::move(comps));
    return mgr.modelById(model_id)->componentIds()[0];
}

std::size_t pointCount(ModelLayer& mgr, Index component_id)
{
    auto op = mgr.getComponentOperator(component_id);
    REQUIRE(op.has_value());
    return op->component().mesh->vertex_positions_.size();
}

//! @brief 目标组件加点的 handler（影子路径的最小写模型）
class AppendPointHandler : public AlgorithmHandler {
public:
    std::any execute(HandlerContext& context, const std::vector<core::ArgObject>&) override
    {
        context.cur_component.appendPoint({ 9.0, 9.0, 9.0 });
        context.report_progress(1.0, "done");
        return { };
    }
    std::vector<core::ArgType> args_type() const override { return { }; }
};

//! @brief 实际选择器解析次数与 inline 所属线程的可观察算法。
class ResolvedInlineHandler : public AlgorithmHandler {
public:
    std::optional<Index> resolveComponentId(ModelLayer&, Index,
        const std::vector<core::ArgObject>& args) const override
    {
        ++resolve_count;
        const auto* selection = args.empty() ? nullptr : args.front().get<ArgTypeEnum::Selector>();
        return selection && *selection ? std::optional<Index> { (*selection)->component_id } : std::nullopt;
    }
    std::any execute(HandlerContext& context, const std::vector<core::ArgObject>&) override
    {
        ++execute_count;
        execution_thread = std::this_thread::get_id();
        executed_target = context.cur_component.componentId();
        context.cur_component.appendPoint({ 9, 0, 0 });
        context.report_progress(0.5, "appended");
        if (fail)
            throw std::runtime_error("inline failure");
        context.report_progress(1.0, "done");
        return std::string { "inline result" };
    }
    std::vector<core::ArgType> args_type() const override { return { }; }
    mutable int resolve_count { 0 };
    int execute_count { 0 };
    Index executed_target { -1 };
    std::thread::id execution_thread;
    bool fail { false };
};

//! @brief 正常入池并建立几何身份，或携带映射，触发真实层 inline 路径。
Index addInlineComponent(ModelLayer& model, bool with_geometry)
{
    auto component = std::make_unique<ComponentData>();
    component->mesh = std::make_unique<MeshData>();
    component->mesh->init();
    component->mesh->vertex_positions_ = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
    if (with_geometry) {
        component->geometry = std::make_unique<GeometryData>();
        component->geometry->setRootShape(GeometryBuilder::makeBox(0, 0, 0, 1, 1, 1));
    } else {
        component->mapping = std::make_unique<GeometryMeshMap>();
    }
    ComponentDatas components;
    components.push_back(std::move(component));
    const auto id = model.addModel("inline target", std::move(components));
    return model.modelById(id)->componentIds().front();
}

//! @brief 先写后抛的 handler：验证失败原子性
class ThrowAfterWriteHandler : public AlgorithmHandler {
public:
    std::any execute(HandlerContext& context, const std::vector<core::ArgObject>&) override
    {
        context.cur_component.appendPoint({ 9.0, 9.0, 9.0 });
        throw std::runtime_error("compute failed");
    }
    std::vector<core::ArgType> args_type() const override { return { }; }
};

//! @brief 经影子 IO 导入新模型的 handler（收养路径）
class ImportingHandler : public AlgorithmHandler {
public:
    std::any execute(HandlerContext& context, const std::vector<core::ArgObject>&) override
    {
        if (!context.io_system.read("dummy.fake", "FakeType", { }))
            throw std::runtime_error("import failed");
        return { };
    }
    std::vector<core::ArgType> args_type() const override { return { }; }
};

//! @brief 导出目标组件的 handler（writeComponents 的 id 映射路径）
class ExportingHandler : public AlgorithmHandler {
public:
    std::any execute(HandlerContext& context, const std::vector<core::ArgObject>&) override
    {
        context.io_system.writeComponents({ context.cur_component.componentId() },
            "out.fake", "FakeType", { });
        return { };
    }
    std::vector<core::ArgType> args_type() const override { return { }; }
};

//! @brief 假格式 handler：read 返回固定载荷，write 记录收到的层与组件 id
class FakeIOHandler : public systems::io::ModelIOHandler {
public:
    std::optional<ModelPayload> read_model(const std::filesystem::path&,
        const std::vector<std::any>&) override
    {
        ++read_count;
        ModelPayload payload;
        payload.model_name = u8"imported";
        auto mesh = std::make_unique<MeshData>();
        mesh->init();
        auto c = std::make_unique<ComponentData>();
        c->name = "imported_comp";
        c->mesh = std::move(mesh);
        payload.components.push_back(std::move(c));
        return payload;
    }

    void write_components(const ModelLayer& mgr, const std::vector<Index>& component_ids,
        const std::filesystem::path&, const std::vector<std::any>&) override
    {
        wrote_model_ = &mgr;
        wrote_ids_ = component_ids;
    }

    std::vector<core::ArgType> read_args_type() const override { return { }; }
    std::vector<core::ArgType> write_args_type() const override { return { }; }

    int read_count { 0 };
    const ModelLayer* wrote_model_ { nullptr };
    std::vector<Index> wrote_ids_;
};

//! @brief 在既有 IO 集成目标内验证注册元数据准备与执行身份。
class RegistrationIOHandler : public FakeIOHandler {
public:
    explicit RegistrationIOHandler(std::string value, int* metadata_calls = nullptr)
        : value(std::move(value))
        , metadata_calls(metadata_calls)
    {
    }
    std::vector<core::ArgType> read_args_type() const override
    {
        if (metadata_calls)
            ++*metadata_calls;
        if (reject_read)
            throw std::runtime_error("read metadata failed");
        return { { ArgTypeEnum::Text, value, { }, { } } };
    }
    std::vector<core::ArgType> write_args_type() const override
    {
        if (metadata_calls)
            ++*metadata_calls;
        if (reject_write)
            throw std::runtime_error("write metadata failed");
        return { { ArgTypeEnum::Text, value, { }, { } } };
    }
    std::string value;
    int* metadata_calls;
    bool reject_read { false };
    bool reject_write { false };
};

void registerFakeIO(ModelIOSystem& io, FakeIOHandler*& out)
{
    systems::io::HandlerMetaData meta;
    meta.file_type = "FakeType";
    meta.extensions = { "fake" };
    out = new FakeIOHandler();
    REQUIRE(io.registerHandler(meta, systems::io::ModelIOSystem::SystemHandlerPtr { out }));
}
}

TEST_CASE("shadow prepare/compute/commit applies mutation with undo", "[AlgorithmShadow]")
{
    CountingObserver obs;
    ModelLayer model(&obs);
    const Index comp = addTriangleComponent(model);
    ModelIOSystem io(model);
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    AlgorithmSystem system(io, model, &undo);

    AppendPointHandler* handler_owned = new AppendPointHandler();
    HandlerMetaData meta;
    meta.name = "AppendPt";
    meta.display_name = "Append Point";
    REQUIRE(system.registerHandler(meta, AlgorithmSystem::SystemHandlerPtr { handler_owned }));

    int heartbeats = 0;
    AsyncRun run(system, model, &undo, "AppendPt", comp, [&](double value, const std::string&) {
        CHECK(value >= 0.0);
        CHECK(value <= 1.0);
        ++heartbeats;
    });
    REQUIRE(heartbeats == 1);
    REQUIRE(model.writesPending());
    REQUIRE(pointCount(model, comp) == 3);
    REQUIRE_FALSE(undo.undo());
    run.complete();
    REQUIRE(pointCount(model, comp) == 4);
    REQUIRE(undo.canUndo());

    // undo 回滚到提交前（restoreSnapshot 的写前标脏 before-image 生效）
    REQUIRE_NOTHROW(undo.undo());
    REQUIRE(pointCount(model, comp) == 3);
}

TEST_CASE("shadow failure leaves model and undo untouched", "[AlgorithmShadow]")
{
    CountingObserver obs;
    ModelLayer model(&obs);
    const Index comp = addTriangleComponent(model);
    ModelIOSystem io(model);
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    AlgorithmSystem system(io, model, &undo);

    HandlerMetaData meta;
    meta.name = "Boom";
    meta.display_name = "Throw After Write";
    REQUIRE(system.registerHandler(meta, AlgorithmSystem::SystemHandlerPtr { new ThrowAfterWriteHandler() }));

    AsyncRun run(system, model, &undo, "Boom", comp);
    REQUIRE(model.writesPending());
    run.completion();
    REQUIRE(run.job->state() == systems::job::JobState::Failed);
    REQUIRE(run.job->error() == "compute failed");
    REQUIRE_FALSE(model.writesPending());
    // 未提交：真实模型零变化、undo 无记录（失败原子性）
    REQUIRE(pointCount(model, comp) == 3);
    REQUIRE_FALSE(undo.canUndo());
    REQUIRE(obs.component_changed_count == 0);
}

TEST_CASE("shadow read imports adopted into real layer with undo", "[AlgorithmShadow]")
{
    CountingObserver obs;
    ModelLayer model(&obs);
    const Index comp = addTriangleComponent(model);
    ModelIOSystem io(model);
    FakeIOHandler* fake = nullptr;
    registerFakeIO(io, fake);
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    AlgorithmSystem system(io, model, &undo);

    HandlerMetaData meta;
    meta.name = "Importer";
    meta.display_name = "Importing";
    REQUIRE(system.registerHandler(meta, AlgorithmSystem::SystemHandlerPtr { new ImportingHandler() }));

    const int added_before = obs.model_added_count;
    AsyncRun run(system, model, &undo, "Importer", comp);
    REQUIRE(obs.model_added_count == added_before);
    run.complete();
    REQUIRE(obs.model_added_count == added_before + 1);
    REQUIRE(undo.canUndo());

    // undo 撤销结构新增：导入的模型被移除
    const Index imported_id = obs.last_model_added;
    REQUIRE(model.modelById(imported_id) != nullptr);
    REQUIRE_NOTHROW(undo.undo());
    REQUIRE(model.modelById(imported_id) == nullptr);
}

TEST_CASE("shadow writeComponents maps shadow id to real id on real layer", "[AlgorithmShadow]")
{
    ModelLayer model;
    const Index comp = addTriangleComponent(model);
    ModelIOSystem io(model);
    FakeIOHandler* fake = nullptr;
    registerFakeIO(io, fake);
    AlgorithmSystem system(io, model, nullptr);

    HandlerMetaData meta;
    meta.name = "Exporter";
    meta.display_name = "Exporting";
    REQUIRE(system.registerHandler(meta, AlgorithmSystem::SystemHandlerPtr { new ExportingHandler() }));

    AsyncRun run(system, model, nullptr, "Exporter", comp);
    REQUIRE(fake->wrote_model_ == &model); // 写真实层（冻结期只读访问）
    REQUIRE(fake->wrote_ids_.size() == 1);
    REQUIRE(fake->wrote_ids_.front() == comp); // 影子 id 已映射回真实 id

    // 只导出不修改：目标未标脏、无导入 → 空操作丢弃
    run.complete();
    REQUIRE(pointCount(model, comp) == 3);
}

TEST_CASE("prepare fails cleanly for unknown algorithm", "[AlgorithmShadow]")
{
    ModelLayer model;
    addTriangleComponent(model);
    ModelIOSystem io(model);
    AlgorithmSystem system(io, model, nullptr);

    OwnerQueue queue;
    systems::job::JobRunner runner(model, nullptr, queue.dispatcher());
    system.setJobRunner(&runner);
    REQUIRE_THROWS_AS(system.callAsync("NoSuchAlgo", 0, { }), std::runtime_error);
    REQUIRE_FALSE(model.writesPending());
    REQUIRE_FALSE(runner.currentJob());
}

TEST_CASE("shadow commit with second component sharing gid space", "[AlgorithmShadow]")
{
    // 多组件场景：目标组件之外存在更高 gid 持有者时，影子分配的新 gid
    // 在 restoreSnapshot reclaim 期是否冲突——本用例探测该边界
    CountingObserver obs;
    ModelLayer model(&obs);
    const Index comp_a = addTriangleComponent(model, "A");
    const Index comp_b = addTriangleComponent(model, "B");
    ModelIOSystem io(model);
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    AlgorithmSystem system(io, model, &undo);

    HandlerMetaData meta;
    meta.name = "AppendPt";
    meta.display_name = "Append Point";
    REQUIRE(system.registerHandler(meta, AlgorithmSystem::SystemHandlerPtr { new AppendPointHandler() }));

    const std::size_t b_before = pointCount(model, comp_b);
    AsyncRun run(system, model, &undo, "AppendPt", comp_a);
    run.complete();
    REQUIRE(pointCount(model, comp_a) == 4);
    REQUIRE(pointCount(model, comp_b) == b_before); // 邻居组件不受影响
}

TEST_CASE("formal algorithm discards preview before resolving and snapshotting target", "[AlgorithmShadow][preview]")
{
    CountingObserver observer;
    ModelLayer model(&observer);
    const Index target = addTriangleComponent(model);
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    ModelIOSystem io(model);
    AlgorithmSystem system(io, model, &undo);
    HandlerMetaData meta;
    meta.name = "AppendPt";
    meta.display_name = "Append Point";
    auto handler = std::make_unique<AppendPointHandler>();
    REQUIRE(system.registerHandler(meta, AlgorithmSystem::SystemHandlerPtr { handler.release() }));
    {
        UndoStack::OwnerScope owner(&undo, "FeaturePreview");
        REQUIRE(undo.beginScope("preview"));
        model.getComponentOperator(target)->appendPoint({ 4, 4, 4 });
    }
    REQUIRE(pointCount(model, target) == 4);
    SECTION("shadow")
    {
        AsyncRun run(system, model, &undo, "AppendPt", target);
        REQUIRE_FALSE(undo.scopeActive());
        REQUIRE(pointCount(model, target) == 3);
        run.complete();
    }
    SECTION("synchronous")
    {
        system.call("AppendPt", target, { });
    }
    REQUIRE_FALSE(model.writesPending());
    REQUIRE_FALSE(undo.scopeActive());
    REQUIRE(pointCount(model, target) == 4);
    REQUIRE(undo.undoLabel() == "Append Point");
    REQUIRE(undo.undo());
    REQUIRE(pointCount(model, target) == 3);
    REQUIRE_FALSE(undo.canUndo());
    REQUIRE(undo.redo());
    REQUIRE(pointCount(model, target) == 4);
}

TEST_CASE("algorithm preparation resolves a target restored by preview rollback", "[AlgorithmShadow][preview]")
{
    CountingObserver observer;
    ModelLayer model(&observer);
    const Index target = addTriangleComponent(model);
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    ModelIOSystem io(model);
    AlgorithmSystem system(io, model, &undo);
    HandlerMetaData meta;
    meta.name = "AppendPt";
    auto handler = std::make_unique<AppendPointHandler>();
    REQUIRE(system.registerHandler(meta, AlgorithmSystem::SystemHandlerPtr { handler.release() }));
    {
        UndoStack::OwnerScope owner(&undo, "Preview");
        REQUIRE(undo.beginScope("隐藏组件"));
        model.removeComponent(target);
    }
    REQUIRE_FALSE(model.findComponent(target));
    AsyncRun run(system, model, &undo, "AppendPt", target);
    REQUIRE(model.findComponent(target));
    REQUIRE_FALSE(undo.scopeActive());
    run.complete();
    REQUIRE(pointCount(model, target) == 4);
    REQUIRE(undo.undo());
    REQUIRE(pointCount(model, target) == 3);
    REQUIRE_FALSE(undo.canUndo());
}

TEST_CASE("Native IO establishes history and rejects busy imports before parsing", "[AlgorithmShadow][IO][operation]")
{
    CountingObserver observer;
    ModelLayer model(&observer);
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    ModelIOSystem io(model, &undo);
    FakeIOHandler* handler;
    registerFakeIO(io, handler);
    REQUIRE(io.read("first.fake", "FakeType", { }));
    REQUIRE(handler->read_count == 1);
    const Index first = observer.last_model_added;
    REQUIRE(model.modelById(first));
    REQUIRE(undo.undo());
    REQUIRE_FALSE(model.modelById(first));
    REQUIRE(undo.redo());
    REQUIRE(model.modelById(first));

    REQUIRE(undo.beginScope("preview"));
    const Index temporary = model.addModel("preview", { });
    REQUIRE(io.read("second.fake", "FakeType", { }));
    REQUIRE_FALSE(undo.scopeActive());
    REQUIRE_FALSE(model.modelById(temporary));
    const Index second = observer.last_model_added;
    REQUIRE(handler->read_count == 2);
    REQUIRE(undo.undo());
    REQUIRE_FALSE(model.modelById(second));
    REQUIRE(model.modelById(first));

    OwnerQueue queue;
    systems::job::JobRunner runner(model, &undo, queue.dispatcher());
    auto job = runner.run("occupy", [&] { return systems::job::JobWork { [](systems::job::ProgressFn) { }, { } }; }, { });
    auto completion = queue.take();
    REQUIRE_THROWS_AS(io.read("busy.fake", "FakeType", { }), ModelOperationBusy);
    REQUIRE(handler->read_count == 2);
    completion();
    REQUIRE_FALSE(model.writesPending());
}

TEST_CASE("IO registration is stable and duplicate formats keep the original handler", "[IO][registration]")
{
    ModelLayer model;
    const Index target = addTriangleComponent(model);
    ModelIOSystem io(model);
    const systems::io::HandlerMetaData meta { "Registered", { "first", "second" } };
    auto original = std::make_unique<RegistrationIOHandler>("original");
    auto* handler = original.get();
    REQUIRE(io.registerHandler(meta, ModelIOSystem::SystemHandlerPtr { original.release() }));
    auto* info = io.registeredFileTypeInfos().front();
    for (int i = 0; i < 256; ++i) {
        const auto name = "Other" + std::to_string(i);
        auto other = std::make_unique<RegistrationIOHandler>(name);
        REQUIRE(io.registerHandler({ name, { name } }, ModelIOSystem::SystemHandlerPtr { other.release() }));
    }
    bool found = false;
    for (auto* entry : io.registeredFileTypeInfos())
        if (entry->name == meta.file_type) {
            REQUIRE(entry == info);
            found = true;
        }
    REQUIRE(found);
    REQUIRE(info->extensions == meta.extensions);
    REQUIRE(info->read_arg_types.front().name == "original");
    REQUIRE(info->write_arg_types.front().name == "original");
    int metadata_calls = 0;
    int notifications = 0;
    io.setOnDialogNameFiltersChanged([&] { ++notifications; });
    auto duplicate = std::make_unique<RegistrationIOHandler>("duplicate", &metadata_calls);
    REQUIRE_FALSE(io.registerHandler({ meta.file_type, { "rejected" } }, ModelIOSystem::SystemHandlerPtr { duplicate.release() }));
    REQUIRE_FALSE(io.registerHandler({ "Null", { } }, { }));
    REQUIRE(metadata_calls == 0);
    REQUIRE(notifications == 0);
    REQUIRE(io.registeredFileTypeInfos().size() == 257);
    REQUIRE(io.parseModel("registered.fake", meta.file_type, { }));
    REQUIRE(handler->read_count == 1);
    io.writeComponents({ target }, "registered.fake", meta.file_type, { });
    REQUIRE(handler->wrote_ids_ == std::vector<Index> { target });
    io.write(model.getComponentOperator(target)->modelId(), "registered.fake", meta.file_type, { });
    REQUIRE(handler->wrote_model_ == &model);
    io.unregisterHandler(meta);
    REQUIRE(io.registeredFileTypeInfos().size() == 256);
    REQUIRE_FALSE(io.parseModel("missing.fake", meta.file_type, { }));
}

TEST_CASE("IO metadata failure leaves no partial registration", "[IO][registration]")
{
    ModelLayer model;
    ModelIOSystem io(model);
    auto original = std::make_unique<RegistrationIOHandler>("original");
    REQUIRE(io.registerHandler({ "Registered", { "original" } }, ModelIOSystem::SystemHandlerPtr { original.release() }));
    const auto infos = io.registeredFileTypeInfos();
    int notifications = 0;
    io.setOnDialogNameFiltersChanged([&] { ++notifications; });
    auto rejected = std::make_unique<RegistrationIOHandler>("rejected");
    SECTION("read metadata") { rejected->reject_read = true; }
    SECTION("write metadata") { rejected->reject_write = true; }
    REQUIRE_THROWS_AS(io.registerHandler({ "Rejected", { "rejected" } }, ModelIOSystem::SystemHandlerPtr { rejected.release() }), std::runtime_error);
    REQUIRE(notifications == 0);
    REQUIRE(io.registeredFileTypeInfos() == infos);
    REQUIRE(infos.front()->extensions == std::vector<std::string> { "original" });
    REQUIRE_FALSE(io.parseModel("rejected.fake", "Rejected", { }));
    REQUIRE(io.parseModel("original.fake", "Registered", { }));
}

TEST_CASE("IO registration stays immutable through preparation computation commit and cleanup", "[IO][operation]")
{
    ModelLayer model;
    ModelIOSystem io(model);
    const systems::io::HandlerMetaData meta { "Registered", { "fake" } };
    auto original = std::make_unique<RegistrationIOHandler>("original");
    auto* handler = original.get();
    REQUIRE(io.registerHandler(meta, ModelIOSystem::SystemHandlerPtr { original.release() }));
    const auto infos = io.registeredFileTypeInfos();
    int metadata_calls = 0;
    int notifications = 0;
    io.setOnDialogNameFiltersChanged([&] { ++notifications; });
    auto check_rejection = [&] {
        CHECK(model.writesPending());
        auto rejected = std::make_unique<RegistrationIOHandler>("rejected", &metadata_calls);
        CHECK_THROWS_AS(io.registerHandler(meta, ModelIOSystem::SystemHandlerPtr { rejected.release() }), ModelOperationBusy);
        CHECK_THROWS_AS(io.unregisterHandler(meta), ModelOperationBusy);
        CHECK(metadata_calls == 0);
        CHECK(notifications == 0);
        CHECK(io.registeredFileTypeInfos() == infos);
    };
    OwnerQueue queue;
    std::binary_semaphore computing(0), resume(0);
    systems::job::JobRunner runner(model, nullptr, queue.dispatcher());
    bool parsed = false;
    std::exception_ptr registration_error, removal_error;
    auto job = runner.run("IO borrow", [&] {
        check_rejection();
        return systems::job::JobWork { [&](ProgressFn) {
                                          try {
                                              auto rejected = std::make_unique<RegistrationIOHandler>("worker", &metadata_calls);
                                              io.registerHandler(meta, ModelIOSystem::SystemHandlerPtr { rejected.release() });
                                          } catch (...) {
                                              registration_error = std::current_exception();
                                          }
                                          try {
                                              io.unregisterHandler(meta);
                                          } catch (...) {
                                              removal_error = std::current_exception();
                                          }
                                          parsed = io.parseModel("worker.fake", meta.file_type, { }).has_value();
                                          computing.release();
                                          resume.acquire();
                                      },
            check_rejection };
    });
    REQUIRE(job);
    CHECK(computing.try_acquire_for(std::chrono::seconds(5)));
    check_rejection();
    CHECK_FALSE(job->isCancellationRequested());
    resume.release();
    auto complete = queue.take();
    check_rejection();
    REQUIRE(runner.deferUntilFinished(check_rejection));
    complete();
    REQUIRE(job->state() == systems::job::JobState::Done);
    REQUIRE(parsed);
    REQUIRE(registration_error);
    REQUIRE(removal_error);
    REQUIRE(handler->read_count == 1);
    REQUIRE_FALSE(model.writesPending());
    REQUIRE_NOTHROW(io.unregisterHandler(meta));
    REQUIRE(io.registeredFileTypeInfos().empty());
    auto replacement = std::make_unique<RegistrationIOHandler>("replacement", &metadata_calls);
    REQUIRE(io.registerHandler(meta, ModelIOSystem::SystemHandlerPtr { replacement.release() }));
    REQUIRE(metadata_calls == 2);
}

TEST_CASE("Inline geometry and mapping algorithms execute the target resolved before start", "[AlgorithmSystem][inline]")
{
    bool with_geometry = false;
    SECTION("geometry") { with_geometry = true; }
    SECTION("mapping") { }
    ModelLayer model;
    const Index target = addInlineComponent(model, with_geometry);
    const Index fallback = addTriangleComponent(model, "fallback");
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    ModelIOSystem io(model, &undo);
    AlgorithmSystem system(io, model, &undo);
    const HandlerMetaData meta { "Inline", "Inline label" };
    auto algorithm = std::make_unique<ResolvedInlineHandler>();
    auto* handler = algorithm.get();
    REQUIRE(system.registerHandler(meta, AlgorithmSystem::SystemHandlerPtr { algorithm.release() }));
    auto selection = std::make_shared<Selection>();
    selection->type = ElementEnum::Component;
    selection->component_id = target;
    const std::vector<core::ArgObject> args { core::ArgObject::create<ArgTypeEnum::Selector>(selection) };
    OwnerQueue queue;
    systems::job::JobRunner runner(model, &undo, queue.dispatcher());
    system.setJobRunner(&runner);
    int starts = 0, finishes = 0;
    std::vector<std::string> progress;
    runner.setOnStarted([&](systems::job::Job& job) {
        ++starts;
        CHECK(job.masked());
        CHECK(model.writesPending());
        selection->component_id = fallback; // 已解析的目标不能随开始观察者的选择变化重新解析。
        CHECK_THROWS_AS(model.getComponentOperator(target)->appendPoint({ 5, 0, 0 }), ModelOperationBusy);
        CHECK_THROWS_AS(system.unregisterHandler(meta), ModelOperationBusy);
        auto replacement = std::make_unique<ResolvedInlineHandler>();
        CHECK_THROWS_AS(system.registerHandler(meta, AlgorithmSystem::SystemHandlerPtr { replacement.release() }), ModelOperationBusy);
    });
    runner.setOnProgress([&](systems::job::Job&, double, const std::string& label) { progress.push_back(label); });
    runner.setOnFinished([&](systems::job::Job&) {
        ++finishes;
        CHECK_FALSE(model.writesPending());
    });
    auto job = system.callAsync(meta.name, fallback, args);
    REQUIRE(job);
    REQUIRE(job->state() == systems::job::JobState::Done);
    REQUIRE(handler->resolve_count == 1);
    REQUIRE(handler->execute_count == 1);
    REQUIRE(handler->execution_thread == std::this_thread::get_id());
    REQUIRE(handler->executed_target == target);
    REQUIRE(starts == 1);
    REQUIRE(finishes == 1);
    REQUIRE(progress == std::vector<std::string> { "appended", "done" });
    REQUIRE(queue.pending.empty());
    REQUIRE(pointCount(model, target) == 4);
    REQUIRE(pointCount(model, fallback) == 3);
    REQUIRE(undo.undoLabel() == meta.display_name);
    REQUIRE(undo.undo());
    REQUIRE(pointCount(model, target) == 3);
    REQUIRE(undo.redo());
    REQUIRE(pointCount(model, target) == 4);
}

TEST_CASE("Inline algorithm failures and cancellation keep partial real effects undoable", "[AlgorithmSystem][inline]")
{
    bool cancel = false;
    SECTION("failure") { }
    SECTION("heartbeat cancellation") { cancel = true; }
    ModelLayer model;
    const Index target = addInlineComponent(model, true);
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    ModelIOSystem io(model, &undo);
    AlgorithmSystem system(io, model, &undo);
    auto algorithm = std::make_unique<ResolvedInlineHandler>();
    auto* handler = algorithm.get();
    handler->fail = !cancel;
    REQUIRE(system.registerHandler({ "Inline", "Inline label" }, AlgorithmSystem::SystemHandlerPtr { algorithm.release() }));
    auto selection = std::make_shared<Selection>();
    selection->type = ElementEnum::Component;
    selection->component_id = target;
    OwnerQueue queue;
    systems::job::JobRunner runner(model, &undo, queue.dispatcher());
    system.setJobRunner(&runner);
    if (cancel)
        runner.setOnProgress([](systems::job::Job& job, double, const std::string&) { job.cancel(); });
    auto job = system.callAsync("Inline", -1, { core::ArgObject::create<ArgTypeEnum::Selector>(selection) });
    REQUIRE(job);
    REQUIRE(job->state() == (cancel ? systems::job::JobState::Cancelled : systems::job::JobState::Failed));
    REQUIRE(job->error() == (cancel ? "" : "inline failure"));
    REQUIRE(handler->resolve_count == 1);
    REQUIRE_FALSE(model.writesPending());
    REQUIRE(queue.pending.empty());
    REQUIRE(pointCount(model, target) == 4);
    REQUIRE(undo.undoLabel() == "Inline label");
    REQUIRE(undo.undo());
    REQUIRE(pointCount(model, target) == 3);
    REQUIRE(undo.redo());
    REQUIRE(pointCount(model, target) == 4);
}

TEST_CASE("Inline execution shares an outer operation and synchronous execution preserves its result", "[AlgorithmSystem][inline]")
{
    ModelLayer model;
    const Index target = addInlineComponent(model, false);
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    ModelIOSystem io(model, &undo);
    AlgorithmSystem system(io, model, &undo);
    auto algorithm = std::make_unique<ResolvedInlineHandler>();
    auto* handler = algorithm.get();
    REQUIRE(system.registerHandler({ "Inline", "Inline label" }, AlgorithmSystem::SystemHandlerPtr { algorithm.release() }));
    auto selection = std::make_shared<Selection>();
    selection->type = ElementEnum::Component;
    selection->component_id = target;
    const std::vector<core::ArgObject> args { core::ArgObject::create<ArgTypeEnum::Selector>(selection) };
    OwnerQueue queue;
    systems::job::JobRunner runner(model, &undo, queue.dispatcher());
    system.setJobRunner(&runner);
    {
        ModelScope outer(model, &undo, "outer", ModelScope::Kind::Execute, "Feature");
        auto job = system.callAsync("Inline", -1, args);
        REQUIRE(job);
        REQUIRE(job->state() == systems::job::JobState::Done);
        REQUIRE(undo.inOperation());
        REQUIRE_FALSE(undo.canUndo());
    }
    REQUIRE(handler->resolve_count == 1);
    REQUIRE(undo.undoLabel() == "outer");
    REQUIRE(undo.undo());
    REQUIRE(pointCount(model, target) == 3);
    REQUIRE(std::any_cast<std::string>(system.call("Inline", -1, args)) == "inline result");
    REQUIRE(handler->resolve_count == 2);
    REQUIRE(undo.undoLabel() == "Inline label");
}

TEST_CASE("Inline algorithms remain available without an undo stack", "[AlgorithmSystem][inline]")
{
    ModelLayer model;
    const Index target = addInlineComponent(model, false);
    ModelIOSystem io(model);
    AlgorithmSystem system(io, model);
    auto algorithm = std::make_unique<ResolvedInlineHandler>();
    auto* handler = algorithm.get();
    REQUIRE(system.registerHandler({ "Inline", "" }, AlgorithmSystem::SystemHandlerPtr { algorithm.release() }));
    auto selection = std::make_shared<Selection>();
    selection->type = ElementEnum::Component;
    selection->component_id = target;
    OwnerQueue queue;
    systems::job::JobRunner runner(model, nullptr, queue.dispatcher());
    system.setJobRunner(&runner);
    auto job = system.callAsync("Inline", -1, { core::ArgObject::create<ArgTypeEnum::Selector>(selection) });
    REQUIRE(job);
    REQUIRE(job->state() == systems::job::JobState::Done);
    REQUIRE(handler->resolve_count == 1);
    REQUIRE(handler->execution_thread == std::this_thread::get_id());
    REQUIRE(queue.pending.empty());
    REQUIRE(pointCount(model, target) == 4);
    REQUIRE_FALSE(model.writesPending());
}
