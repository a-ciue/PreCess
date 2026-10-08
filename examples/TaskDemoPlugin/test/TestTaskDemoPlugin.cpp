/** @file TestTaskDemoPlugin.cpp
 * @brief 无网格回写任务的准备异常及占用释放回归测试
 */
#include "ComponentData.h"
#include "EventBus.h"
#include "FeatureSystem.h"
#include "JobRunner.h"
#include "ModelLayer.h"
#include "TaskDemoHandler.h"
#include "UndoStack.h"
#include "OwnerQueue.h"
#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

TEST_CASE("TaskDemo writeback rejects components without meshes and releases the slot", "[TaskDemoPlugin]")
{
    ModelLayer model;
    ComponentDatas components;
    components.push_back(std::make_unique<ComponentData>());
    const auto mid = model.addModel("no mesh", std::move(components));
    const auto cid = model.modelById(mid)->componentIds()[0];
    UndoStack undo(model);
    model.setUndoRecorder(&undo);
    core::EventBus bus;
    OwnerQueue queue;
    systems::job::JobRunner runner(model, &undo, queue.dispatcher());
    systems::feature::FeatureSystem system(model, bus, &undo);
    system.setJobRunner(&runner);
    system.setActiveComponentProvider([cid] { return std::optional<Index> { cid }; });
    systems::feature::HandlerMetaData meta;
    meta.name = "TaskDemo";
    REQUIRE(system.registerHandler(meta, systems::feature::FeatureSystem::SystemHandlerPtr { std::make_unique<systems::feature::TaskDemoHandler>().release() }));
    REQUIRE(system.setParameter("TaskDemo", 0, core::ArgObject::create<ArgTypeEnum::Int>(2)));
    REQUIRE_THROWS_AS(system.invoke("TaskDemo"), std::runtime_error);
    CHECK_FALSE(model.writesPending());
    CHECK_FALSE(runner.currentJob());
    CHECK_FALSE(undo.canUndo());
    REQUIRE(model.findComponent(cid));
    CHECK_FALSE(model.findComponent(cid)->mesh);
    REQUIRE(system.setParameter("TaskDemo", 0, core::ArgObject::create<ArgTypeEnum::Int>(0)));
    system.invoke("TaskDemo");
    REQUIRE(runner.currentJob());
    runner.currentJob()->cancel();
    queue.take()();
    CHECK_FALSE(model.writesPending());
    runner.stop();
}
