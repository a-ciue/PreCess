#include "AlgorithmHandler.h"
#include "AlgorithmSystem.h"
#include "AlgorithmSystemRegister.h"
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"

#include "ArgObject.h"
#include "ComponentData.h"
#include "JobRunner.h"
#include "MeshData.h"
#include "ModelIOSystem.h"
#include "ModelLayer.h"
#include "UndoStack.h"
#include "test/OwnerQueue.h"
#include <QJsonArray>

#include <array>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <exception>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace systems::algo;
using Catch::Approx;

namespace {
//! @brief 上报两段进度的算法 handler，用于验证 ProgressFn 注入链路
class ReportingHandler : public AlgorithmHandler {
public:
    std::any execute(HandlerContext& context, const std::vector<core::ArgObject>& /*args*/) override
    {
        context.report_progress(0.3, "stage-a");
        context.report_progress(1.0, "stage-b");
        executed = true;
        return { };
    }

    std::vector<core::ArgType> args_type() const override
    {
        return { };
    }

    bool executed { false }; //!< 执行标记：验证未注入回调时 handler 照常执行（report 走 no-op）
};

//! @brief 可观察注册信息、执行身份和元数据异常的算法。
class RegistrationHandler : public AlgorithmHandler {
public:
    explicit RegistrationHandler(std::string value)
        : value(std::move(value))
    {
    }
    std::any execute(HandlerContext& context, const std::vector<core::ArgObject>&) override
    {
        context.cur_component.appendPoint({ 9, 0, 0 });
        return value;
    }
    std::vector<core::ArgType> args_type() const override
    {
        if (metadata_calls)
            ++*metadata_calls;
        if (reject_metadata)
            throw std::runtime_error("metadata failed");
        return { { ArgTypeEnum::Text, value, { }, { } } };
    }
    std::string value;
    bool reject_metadata { false };
    int* metadata_calls { nullptr };
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

    const Index model_id = mgr.addModel("algo_progress_test", std::move(comps));
    return mgr.modelById(model_id)->componentIds()[0];
}
}

TEST_CASE("AlgorithmSystem::call forwards progress to injected callback", "[AlgorithmSystem]")
{
    ModelLayer model_manager;
    systems::io::ModelIOSystem io_system(model_manager);
    AlgorithmSystem system(io_system, model_manager, nullptr);
    const Index component_id = addTriangleComponent(model_manager);

    auto* handler_owned = new ReportingHandler();
    HandlerMetaData meta_data;
    meta_data.name = "Reporting";
    meta_data.display_name = "Reporting Handler";
    REQUIRE(system.registerHandler(meta_data, AlgorithmSystem::SystemHandlerPtr { handler_owned }));

    // 注入回调：按序收到 handler 的两段上报
    std::vector<std::pair<double, std::string>> reports;
    REQUIRE_NOTHROW(system.call("Reporting", component_id, { },
        [&reports](double value, const std::string& label) { reports.emplace_back(value, label); }));
    REQUIRE(handler_owned->executed);
    REQUIRE(reports.size() == 2);
    REQUIRE(reports[0].first == Approx(0.3));
    REQUIRE(reports[0].second == "stage-a");
    REQUIRE(reports[1].first == Approx(1.0));
    REQUIRE(reports[1].second == "stage-b");

    // 未注入回调（默认参数 / 空回调）：report 走 no-op 默认值，handler 不判空也不崩
    handler_owned->executed = false;
    REQUIRE_NOTHROW(system.call("Reporting", component_id, { }));
    REQUIRE(handler_owned->executed);
}

TEST_CASE("Algorithm registration keeps stable information and replaces handler with metadata", "[AlgorithmSystem][registration]")
{
    ModelLayer model;
    const Index target = addTriangleComponent(model);
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    systems::io::ModelIOSystem io(model, &undo);
    AlgorithmSystem system(io, model, &undo);
    HandlerMetaData meta { "Registered", "Original label" };
    auto first = std::make_unique<RegistrationHandler>("first");
    auto* first_handler = first.get();
    REQUIRE(system.registerHandler(meta, AlgorithmSystem::SystemHandlerPtr { first.release() }));
    auto* first_info = system.getAlgorithmInfos().front();
    auto find_info = [&](const std::string& name) -> AlgorithmInfo* {
        for (auto* info : system.getAlgorithmInfos())
            if (info->name == name)
                return info;
        return nullptr;
    };
    for (int i = 0; i < 256; ++i) {
        const auto name = "Other" + std::to_string(i);
        auto handler = std::make_unique<RegistrationHandler>(name);
        REQUIRE(system.registerHandler({ name, name }, AlgorithmSystem::SystemHandlerPtr { handler.release() }));
    }
    REQUIRE(find_info(meta.name) == first_info);
    REQUIRE(first_info->display_name == "Original label");
    REQUIRE(first_info->arg_types.front().name == "first");
    // 参数查询仍调用插件，不借注册信息合并改成静态缓存。
    first_handler->value = "changed";
    REQUIRE(system.getArgTypes(meta.name)->front().name == "changed");
    REQUIRE(first_info->arg_types.front().name == "first");
    REQUIRE(std::any_cast<std::string>(system.call(meta.name, target, { })) == "changed");
    REQUIRE(undo.undoLabel() == "Original label");

    auto second = std::make_unique<RegistrationHandler>("second");
    meta.display_name.clear();
    REQUIRE(system.registerHandler(meta, AlgorithmSystem::SystemHandlerPtr { second.release() }));
    REQUIRE(system.getAlgorithmInfos().size() == 257);
    auto* replacement_info = find_info(meta.name);
    REQUIRE(replacement_info);
    REQUIRE(replacement_info->display_name.empty());
    REQUIRE(replacement_info->arg_types.front().name == "second");
    REQUIRE(system.getArgTypes(meta.name)->front().name == "second");
    REQUIRE(std::any_cast<std::string>(system.call(meta.name, target, { })) == "second");
    REQUIRE(undo.undoLabel() == meta.name);
    REQUIRE(undo.undo());
    REQUIRE(undo.undoLabel() == "Original label");
    REQUIRE(undo.undo());
    REQUIRE(model.findComponent(target)->mesh->vertex_positions_.size() == 3);

    system.unregisterHandler(meta);
    REQUIRE_FALSE(find_info(meta.name));
    REQUIRE_FALSE(system.getArgTypes(meta.name));
    REQUIRE(system.getAlgorithmInfos().size() == 256);
    REQUIRE(std::any_cast<std::string>(system.call("Other255", target, { })) == "Other255");
}

TEST_CASE("Algorithm metadata failure leaves existing registrations intact", "[AlgorithmSystem][registration]")
{
    ModelLayer model;
    const Index target = addTriangleComponent(model);
    systems::io::ModelIOSystem io(model);
    AlgorithmSystem system(io, model);
    auto original = std::make_unique<RegistrationHandler>("original");
    REQUIRE(system.registerHandler({ "Registered", "Original label" }, AlgorithmSystem::SystemHandlerPtr { original.release() }));
    const auto infos = system.getAlgorithmInfos();
    int notifications = 0;
    system.setOnAlgorithmInfosChanged([&] { ++notifications; });
    HandlerMetaData meta { "Registered", "Replacement label" };
    SECTION("failed replacement") { }
    SECTION("failed new registration") { meta.name = "New"; }
    auto rejected = std::make_unique<RegistrationHandler>("rejected");
    rejected->reject_metadata = true;
    REQUIRE_THROWS_AS(system.registerHandler(meta, AlgorithmSystem::SystemHandlerPtr { rejected.release() }), std::runtime_error);
    REQUIRE(notifications == 0);
    REQUIRE(system.getAlgorithmInfos() == infos);
    REQUIRE(infos.front()->name == "Registered");
    REQUIRE(infos.front()->display_name == "Original label");
    REQUIRE(infos.front()->arg_types.front().name == "original");
    REQUIRE_FALSE(system.getArgTypes("New"));
    REQUIRE(system.getArgTypes("Registered")->front().name == "original");
    REQUIRE(std::any_cast<std::string>(system.call("Registered", target, { })) == "original");
}

TEST_CASE("Algorithm registration and runner binding reject wrong threads before side effects", "[AlgorithmSystem][registration][thread]")
{
    ModelLayer model;
    const Index target = addTriangleComponent(model);
    systems::io::ModelIOSystem io(model);
    OwnerQueue queue;
    systems::job::JobRunner runner(model, nullptr, queue.dispatcher());
    AlgorithmSystem system(io, model);
    const HandlerMetaData meta { "Registered", "Original" };
    auto original = std::make_unique<RegistrationHandler>("original");
    REQUIRE(system.registerHandler(meta, AlgorithmSystem::SystemHandlerPtr { original.release() }));
    system.setJobRunner(&runner);
    const auto infos = system.getAlgorithmInfos();
    int metadata_calls = 0;
    int notifications = 0;
    system.setOnAlgorithmInfosChanged([&] { ++notifications; });
    std::array<std::exception_ptr, 4> errors;
    std::thread wrong_thread([&] {
        const std::array<std::function<void()>, 4> actions {
            [&] {
                auto replacement = std::make_unique<RegistrationHandler>("replacement");
                replacement->metadata_calls = &metadata_calls;
                system.registerHandler(meta, AlgorithmSystem::SystemHandlerPtr { replacement.release() });
            },
            [&] { system.unregisterHandler(meta); },
            [&] { system.setJobRunner(nullptr); },
            [&] { system.setJobRunner(&runner); }
        };
        for (std::size_t i = 0; i < actions.size(); ++i) {
            try {
                actions[i]();
            } catch (...) {
                errors[i] = std::current_exception();
            }
        }
    });
    wrong_thread.join();
    for (auto error : errors) {
        REQUIRE(error);
        REQUIRE_THROWS_AS(std::rethrow_exception(error), std::runtime_error);
    }
    REQUIRE(metadata_calls == 0);
    REQUIRE(notifications == 0);
    REQUIRE(system.getAlgorithmInfos() == infos);
    REQUIRE(infos.front()->display_name == "Original");
    {
        auto occupation = model.beginWriteOperation();
        REQUIRE(occupation);
        ModelLayer::WritePrivilege privilege(model, occupation.get());
        REQUIRE_THROWS_AS(system.setJobRunner(nullptr), ModelOperationBusy);
        REQUIRE_THROWS_AS(system.setJobRunner(&runner), ModelOperationBusy);
    }
    // 被拒的解绑不丢失原执行器；原注册仍能执行并提交影子结果。
    auto job = system.callAsync(meta.name, target, { });
    REQUIRE(job);
    queue.take()();
    REQUIRE(job->state() == systems::job::JobState::Done);
    REQUIRE(model.findComponent(target)->mesh->vertex_positions_.size() == 4);
    REQUIRE_NOTHROW(system.setJobRunner(nullptr));
    REQUIRE_NOTHROW(system.unregisterHandler(meta));
    REQUIRE(system.getAlgorithmInfos().empty());
}
