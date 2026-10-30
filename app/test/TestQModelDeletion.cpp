/** @file TestQModelDeletion.cpp
 * @brief Qt 删除入口的忙时拒绝与撤销状态通知回归测试
 */
#include "ComponentData.h"
#include "ComponentOperator.h"
#include "FeatureContext.h"
#include "FeatureHandler.h"
#include "FeatureSystem.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "QModelManager.h"
#include "UndoStack.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <future>
#include <thread>

TEST_CASE("Qt deletion refuses occupied operations without emitting removal", "[QModelManager]")
{
    int argc = 1;
    char name[] = "TestQModelDeletion";
    char* argv[] = { name, nullptr };
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    QModelManager manager((directory.path() + "/isolated/Test.exe").toStdString());
    auto& model = *manager.getModelManager();
    // 初始数据不记入测试关注的删除历史。
    model.setUndoRecorder(nullptr);
    ComponentDatas components;
    components.push_back(std::make_unique<ComponentData>());
    const auto mid = model.addModel("fixture", std::move(components));
    const auto cid = model.modelById(mid)->componentIds()[0];
    int removed = 0;
    QObject::connect(&manager, &QModelManager::modelRemoved, [&] { ++removed; });
    auto operation = model.beginWriteOperation(false);
    CHECK_NOTHROW(manager.removeModel(mid));
    CHECK_NOTHROW(manager.removeComponent(cid));
    CHECK(removed == 0);
    CHECK(model.modelById(mid));
    CHECK(model.findComponent(cid));
    operation.reset();
    CHECK_NOTHROW(manager.removeComponent(cid));
    CHECK_FALSE(model.findComponent(cid));
    // 不经适配器发命令的修改也应通过模型通知转发删除信号。
    model.removeModel(mid);
    CHECK(removed == 1);
}

TEST_CASE("Qt undo adaptor publishes opening an empty preview", "[QUndoStackAdaptor]")
{
    ModelLayer model;
    UndoStack undo(model);
    QUndoStackAdaptor adaptor(undo);
    int changed = 0;
    QObject::connect(&adaptor, &QUndoStackAdaptor::stackChanged, [&] {
        ++changed;
        CHECK(adaptor.scopeActive());
        CHECK(adaptor.canUndo());
        CHECK_FALSE(adaptor.canRedo());
        CHECK(adaptor.undoLabel() == "preview");
    });
    REQUIRE(undo.beginScope("preview"));
    CHECK(changed == 1);
    undo.setOnChanged({ });
}

TEST_CASE("Qt model shutdown completes feature cleanup without refreshing the UI", "[QModelManager][shutdown]")
{
    int argc = 1;
    char name[] = "TestQModelShutdown";
    char* argv[] = { name, nullptr };
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    bool cleanup_completed = false;
    auto manager = std::make_unique<QModelManager>((directory.path() + "/isolated/Test.exe").toStdString());
    auto& model = *manager->getModelManager();
    /** @brief 模拟功能退出时清理组件数据，触发与视口刷新相同的组件通知。 */
    class ShutdownCleanup final : public systems::feature::FeatureHandler {
    public:
        /** @brief 保存组件身份及独立于会话的清理证据。 */
        explicit ShutdownCleanup(bool& completed)
            : completed_(completed)
        {
        }
        /** @brief 本夹具不需要界面参数。 */
        void setup(systems::feature::FeatureRegistrar&, systems::feature::FeatureContext&) override { }
        /** @brief 经正式操作安装带网格的组件，正常运行时仍应通知界面。 */
        std::any execute(systems::feature::FeatureContext& context) override
        {
            ComponentDatas components;
            auto component = std::make_unique<ComponentData>();
            component->mesh = std::make_unique<MeshData>();
            components.push_back(std::move(component));
            const auto model_id = context.model.addModel("shutdown fixture", std::move(components));
            component_id_ = context.model.modelById(model_id)->componentIds()[0];
            context.model.getComponentOperator(component_id_)->setName("active");
            return { };
        }
        /** @brief 模型清理必须完成，但此时不应再通知正在销毁的界面。 */
        void teardown(systems::feature::FeatureContext& context) override
        {
            context.model.getComponentOperator(component_id_)->setName("closed");
            completed_ = true;
        }

    private:
        bool& completed_;
        Index component_id_ { -1 };
    };
    auto& system = *manager->getFeatureSystemAdaptor()->featureSystem();
    REQUIRE(system.registerHandler({ .name = "ShutdownCleanup" },
        systems::feature::FeatureSystem::SystemHandlerPtr { std::make_unique<ShutdownCleanup>(cleanup_completed).release() }));
    int component_updates = 0;
    int stack_updates = 0;
    QObject::connect(manager->getModelObserver(), &QModelObserver::componentChanged, &app, [&](Index) { ++component_updates; });
    QObject::connect(manager->getUndoStackAdaptor(), &QUndoStackAdaptor::stackChanged, &app, [&] { ++stack_updates; });
    system.invoke("ShutdownCleanup");
    REQUIRE(component_updates > 0);
    REQUIRE(stack_updates > 0);
    component_updates = 0;
    stack_updates = 0;
    manager.reset();
    CHECK(cleanup_completed);
    CHECK(component_updates == 0);
    CHECK(stack_updates == 0);
}

TEST_CASE("Qt model shutdown stops the worker without publishing task updates", "[QModelManager][shutdown]")
{
    int argc = 1;
    char name[] = "TestQModelShutdownJob";
    char* argv[] = { name, nullptr };
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    /** @brief 独立于会话存活，记录工作线程是否实际退出及任务最终状态。 */
    struct JobState {
        std::promise<void> started;
        std::atomic_bool exited { false };
        std::shared_ptr<systems::job::Job> job;
    } state;
    /** @brief 发布带取消检查的真实任务，关闭会话必须等待它退出。 */
    class ShutdownJob final : public systems::feature::FeatureHandler {
    public:
        /** @brief 将退出证据保存到测试持有的状态中。 */
        explicit ShutdownJob(JobState& state)
            : state_(state)
        {
        }
        /** @brief 此任务不需要界面参数。 */
        void setup(systems::feature::FeatureRegistrar&, systems::feature::FeatureContext&) override { }
        /** @brief 等待实际取消检查，不用固定延时假定工作线程已开始。 */
        std::any execute(systems::feature::FeatureContext& context) override
        {
            state_.job = context.runComputeJob("关闭任务", [this](systems::job::ProgressFn report) {
                state_.started.set_value();
                try {
                    for (;;) {
                        report(0.5, "等待关闭");
                        std::this_thread::yield();
                    }
                } catch (...) {
                    state_.exited.store(true);
                    throw;
                }
            });
            return { };
        }

    private:
        JobState& state_;
    };
    int task_updates = 0;
    auto manager = std::make_unique<QModelManager>((directory.path() + "/isolated/Test.exe").toStdString());
    auto& system = *manager->getFeatureSystemAdaptor()->featureSystem();
    REQUIRE(system.registerHandler({ .name = "ShutdownJob" },
        systems::feature::FeatureSystem::SystemHandlerPtr { std::make_unique<ShutdownJob>(state).release() }));
    REQUIRE(system.setFeatureActive("ShutdownJob"));
    QObject::connect(manager->getTaskStatus(), &QTaskStatus::runningChanged, &app, [&] { ++task_updates; });
    auto started = state.started.get_future();
    system.invoke("ShutdownJob");
    REQUIRE(state.job);
    REQUIRE(started.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    REQUIRE(task_updates == 1);
    manager.reset();
    CHECK(state.exited.load());
    CHECK(state.job->state() == systems::job::JobState::Cancelled);
    CHECK(task_updates == 1);
}
