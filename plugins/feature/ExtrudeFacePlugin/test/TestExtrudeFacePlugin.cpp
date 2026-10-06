#include "ComponentData.h"
#include "EventBus.h"
#include "ExtrudeFaceHandler.h"
#include "FeatureEvents.h"
#include "FeatureSystem.h"
#include "GeometryBuilder.h"
#include "GeometryData.h"
#include "JobRunner.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "ModelOperator.h"
#include "Selection.h"
#include "UndoStack.h"
#include "test/OwnerQueue.h"

#include <TopExp_Explorer.hxx>

#include <catch2/catch_test_macros.hpp>

#include <any>
#include <thread>

using namespace systems;
using namespace systems::feature;

namespace {
HandlerMetaData handlerMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "ExtrudeFace";
    meta_data.display_name = "拉伸面为实体";
    return meta_data;
}

//! @brief 临时模型承载一个长方体几何组件，并建好几何索引
Index addBoxGeometryComponent(ModelLayer& model_layer)
{
    const Index model_id = model_layer.addModel("temp_Fixture", {});
    auto model_operator = model_layer.getModelOperator(model_id);
    REQUIRE(model_operator.has_value());
    auto geometry = std::make_unique<GeometryData>();
    geometry->setRootShape(GeometryBuilder::makeBox(0.0, 0.0, 0.0, 10.0, 10.0, 10.0));
    auto component = std::make_unique<ComponentData>();
    component->name = "Fixture";
    component->geometry = std::move(geometry);
    const Index component_id = model_operator->addGeometryComponent(std::move(component));
    model_layer.findComponent(component_id)->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    return component_id;
}
}

TEST_CASE("ExtrudeFace execute appends extruded solid to source component", "[ExtrudeFacePlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    const Index component_id = addBoxGeometryComponent(model_layer);
    UndoStack undo(model_layer);
    model_layer.setUndoRecorder(&undo);
    OwnerQueue queue;
    job::JobRunner runner(model_layer, &undo, queue.dispatcher());
    FeatureSystem feature_system(model_layer, bus, &undo);
    feature_system.setJobRunner(&runner);
    feature_system.setActiveComponentProvider([] { return std::optional<Index> { 999 }; });

    FeatureSystem::SystemHandlerPtr handler { new ExtrudeFaceHandler };
    REQUIRE(feature_system.registerHandler(handlerMetaData(), std::move(handler)));

    // 未选择截面时返回界面提示语
    const std::any hint = feature_system.invoke("ExtrudeFace");
    REQUIRE(std::any_cast<std::string>(hint) == "请选择一个几何面。");

    // 选择长方体顶面（沿 Z 正向拉伸，长度取默认 10）
    auto* component = model_layer.findComponent(component_id);
    const auto& face_ids = component->geometry->index.face_local_to_global;
    REQUIRE(face_ids.size() == 6 + 1); // 局部 id 从 1 起，0 号为保留槽
    auto selection = std::make_shared<Selection>();
    selection->type = ElementEnum::GeometryFace;
    selection->ids = { face_ids[1] }; // 下标从 1 起，0 号为保留槽
    selection->component_id = component_id;
    SECTION("explicit component identity") { }
    SECTION("global face identity without component hint") { selection->component_id = -1; }
    REQUIRE(feature_system.setParameter("ExtrudeFace", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(selection)));

    const auto old_face_ids = component->geometry->index.face_local_to_global;
    const auto old_solid_ids = component->geometry->index.solid_local_to_global;
    const TopoDS_Shape old_root = *component->geometry->rootShape;
    const auto owner_thread = std::this_thread::get_id();
    std::thread::id compute_thread;
    double latest_progress = 0;
    int results = 0;
    runner.setOnProgress([&](job::Job& task, double progress, const std::string& text) {
        if (task.state() == job::JobState::Running) {
            compute_thread = std::this_thread::get_id();
            latest_progress = progress;
        } else {
            REQUIRE(task.state() == job::JobState::Committing);
            REQUIRE(std::this_thread::get_id() == owner_thread);
            REQUIRE(text.find("拉伸完成") != std::string::npos);
            REQUIRE(progress == 1.0);
            ++results;
        }
    });
    REQUIRE(std::any_cast<std::string>(feature_system.invoke("ExtrudeFace")) == "正在计算拉伸…");
    auto task = runner.currentJob();
    REQUIRE(task);
    REQUIRE(model_layer.writesFrozen());
    REQUIRE_FALSE(undo.inOperation());
    REQUIRE_FALSE(undo.canUndo());
    REQUIRE_FALSE(undo.undo());
    REQUIRE_THROWS(model_layer.getComponentOperator(component_id)->setName("busy"));
    auto completion = queue.take();
    REQUIRE(compute_thread != owner_thread);
    REQUIRE(latest_progress == 1.0);
    REQUIRE(component->geometry->index.solid_local_to_global == old_solid_ids);
    REQUIRE(results == 0);
    completion();
    REQUIRE(task->state() == job::JobState::Done);
    REQUIRE_FALSE(model_layer.writesPending());
    REQUIRE(results == 1);

    // 源面保留，拉伸结果以截面副本追加（拓扑独立）：6 源面 + 6 棱柱面，2 个实体
    component->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    REQUIRE(component->geometry->index.solid_local_to_global.size() == 1 + 1 + 1);
    REQUIRE(component->geometry->index.face_local_to_global.size() == 12 + 1);
    int old_solids = 0;
    for (TopExp_Explorer explorer(old_root, TopAbs_SOLID); explorer.More(); explorer.Next())
        ++old_solids;
    REQUIRE(old_solids == 1); // 共享旧根没有被计算或追加原地改变。
    REQUIRE(undo.undoLabel() == "拉伸面为实体");
    for (int i = 0; i < 2; ++i) {
        REQUIRE(undo.undo());
        component = model_layer.findComponent(component_id);
        REQUIRE(component->geometry->index.face_local_to_global == old_face_ids);
        REQUIRE(component->geometry->index.solid_local_to_global == old_solid_ids);
        REQUIRE_FALSE(undo.canUndo());
        REQUIRE(undo.redo());
        component = model_layer.findComponent(component_id);
        REQUIRE(component->geometry->index.solid_local_to_global.size() == 3);
    }
}

TEST_CASE("ExtrudeFace cancellation and compute failure discard the result", "[ExtrudeFacePlugin][job]")
{
    ModelLayer model;
    const auto target = addBoxGeometryComponent(model);
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    OwnerQueue queue;
    job::JobRunner runner(model, &undo, queue.dispatcher());
    core::EventBus bus;
    FeatureSystem system(model, bus, &undo);
    system.setJobRunner(&runner);
    REQUIRE(system.registerHandler(handlerMetaData(), FeatureSystem::SystemHandlerPtr { new ExtrudeFaceHandler }));
    const auto face_ids = model.findComponent(target)->geometry->index.face_local_to_global;
    auto selection = std::make_shared<Selection>();
    selection->type = ElementEnum::GeometryFace;
    selection->ids = { face_ids[1] }; // 未携带组件、对象树也未选中，按全局几何身份解析。
    REQUIRE(system.setParameter("ExtrudeFace", 0, core::ArgObject::create<ArgTypeEnum::Selector>(selection)));
    bool invalid_length = false;
    bool early_cancel = false;
    bool missing_runner = false;
    SECTION("cancel running calculation") { early_cancel = true; }
    SECTION("cancel queued commit") { }
    SECTION("invalid length fails in worker") { invalid_length = true; }
    SECTION("missing executor refuses publication") { missing_runner = true; }
    if (invalid_length)
        REQUIRE(system.setParameter("ExtrudeFace", 4, core::ArgObject::create<ArgTypeEnum::Float>(0.0)));
    if (missing_runner)
        system.setJobRunner(nullptr);
    int results = 0;
    runner.setOnProgress([&](job::Job& task, double, const std::string&) {
        if (task.state() == job::JobState::Committing)
            ++results;
    });
    const auto response = std::any_cast<std::string>(system.invoke("ExtrudeFace"));
    auto task = runner.currentJob();
    if (missing_runner) {
        REQUIRE_FALSE(task);
        REQUIRE(response.find("无法启动") != std::string::npos);
    } else {
        REQUIRE(task);
        if (early_cancel)
            task->cancel();
        auto completion = queue.take();
        if (!invalid_length)
            task->cancel();
        REQUIRE(model.writesPending());
        completion();
        REQUIRE(task->state() == (invalid_length ? job::JobState::Failed : job::JobState::Cancelled));
    }
    REQUIRE_FALSE(model.writesPending());
    REQUIRE(model.findComponent(target)->geometry->index.face_local_to_global == face_ids);
    REQUIRE(model.findComponent(target)->geometry->index.solid_local_to_global.size() == 2);
    REQUIRE_FALSE(undo.canUndo());
    REQUIRE(results == 0);
}
