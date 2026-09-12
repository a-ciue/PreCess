/**
 * @file Session.cpp
 * @brief 会话组合根实现：子系统装配、观察者转发桥与有序拆解
 */
#include "Session.h"
#include "AlgorithmSystem.h"
#include "AlgorithmSystemRegister.h"
#include "EditSystem.h"
#include "EditSystemRegister.h"
#include "FeatureEvents.h"
#include "FeatureSystem.h"
#include "FeatureSystemRegister.h"
#include "ModelIOSystem.h"
#include "ModelIOSystemRegister.h"
#include "ModelObserver.h"
#include "SystemPluginManager.h"
#include "UndoStack.h"

#include <spdlog/spdlog.h>

namespace session {

/**
 * @brief 内部转发观察者：宿主观察者（可为 nullptr）照常收到模型层通知，
 *        同时把通知桥接为 ModelEvent 发布到事件总线（与原 app 层 Qt connect 等价）
 */
class Session::ObserverRelay : public ModelObserver {
public:
    ObserverRelay(ModelObserver* user, core::EventBus& bus)
        : user_(user)
        , bus_(bus)
    {
    }

    void notifyModelAdded(Index model_id) override
    {
        if (user_)
            user_->notifyModelAdded(model_id);
        bus_.publish(systems::feature::ModelEvent { systems::feature::ModelEvent::Kind::ModelAdded, model_id, -1 });
    }

    void notifyModelRemoved(Index model_id) override
    {
        if (user_)
            user_->notifyModelRemoved(model_id);
        bus_.publish(systems::feature::ModelEvent { systems::feature::ModelEvent::Kind::ModelRemoved, model_id, -1 });
    }

    void notifyModelChanged(Index model_id) override
    {
        if (user_)
            user_->notifyModelChanged(model_id);
        bus_.publish(systems::feature::ModelEvent { systems::feature::ModelEvent::Kind::ModelChanged, model_id, -1 });
    }

    void notifyModelNameChanged(Index model_id, const std::string& new_name) override
    {
        if (user_)
            user_->notifyModelNameChanged(model_id, new_name);
        bus_.publish(systems::feature::ModelEvent { systems::feature::ModelEvent::Kind::ModelNameChanged, model_id, -1 });
    }

    void notifyComponentChanged(Index component_id) override
    {
        if (user_)
            user_->notifyComponentChanged(component_id);
        bus_.publish(systems::feature::ModelEvent { systems::feature::ModelEvent::Kind::ComponentChanged, -1, component_id });
    }

    void notifyComponentRemoved(Index component_id) override
    {
        if (user_)
            user_->notifyComponentRemoved(component_id);
        bus_.publish(systems::feature::ModelEvent { systems::feature::ModelEvent::Kind::ComponentRemoved, -1, component_id });
    }

    void notifyGeometryLoadFailed(const std::string& message) override
    {
        if (user_)
            user_->notifyGeometryLoadFailed(message);
    }

private:
    ModelObserver* user_; //> 宿主观察者（不拥有）
    core::EventBus& bus_; //> ModelEvent 发布目标
};

Session::Session(ModelObserver* observer)
    : relay_(std::make_unique<ObserverRelay>(observer, event_bus_))
    , model_(relay_.get())
    , query_(model_)
    , undo_stack_(std::make_unique<UndoStack>(model_))
    , io_system_(std::make_unique<systems::io::ModelIOSystem>(model_))
    , algo_system_(std::make_unique<systems::algo::AlgorithmSystem>(*io_system_, model_, undo_stack_.get()))
    , edit_system_(std::make_unique<systems::edit::EditSystem>(model_, undo_stack_.get()))
    , feature_system_(std::make_unique<systems::feature::FeatureSystem>(model_, event_bus_, undo_stack_.get()))
    , plugin_manager_(std::make_unique<systems::SystemPluginManager>())
{
    // undo 栈挂接模型层记录钩子（写前/结构操作时机回调）
    model_.setUndoRecorder(undo_stack_.get());

    // 注册系统：插件按元数据的 system 字段路由到对应注册器
    plugin_manager_->addSystemRegister(systems::io::ModelIOSystem::name, std::make_unique<systems::io::ModelIOSystemRegister>(*io_system_));
    plugin_manager_->addSystemRegister(systems::algo::AlgorithmSystem::name, std::make_unique<systems::algo::AlgorithmSystemRegister>(*algo_system_));
    plugin_manager_->addSystemRegister(systems::edit::EditSystem::name, std::make_unique<systems::edit::EditSystemRegister>(*edit_system_));
    plugin_manager_->addSystemRegister(systems::feature::FeatureSystem::name, std::make_unique<systems::feature::FeatureSystemRegister>(*feature_system_));
}

Session::~Session()
{
    teardown();
}

void Session::loadStaticPlugins()
{
    plugin_manager_->registerStaticPlugins();
}

void Session::loadPluginsFromDirectory(const std::filesystem::path& plugin_dir)
{
    if (!std::filesystem::is_directory(plugin_dir)) {
        spdlog::info("Session::loadPluginsFromDirectory: 插件目录 {} 不存在，跳过动态插件加载", plugin_dir.string());
        return;
    }
    for (const auto& entry : std::filesystem::directory_iterator(plugin_dir)) {
        if (entry.is_regular_file())
            plugin_manager_->registerPlugin(entry.path());
    }
}

void Session::removeModel(Index model_id)
{
    model_.removeModel(model_id);
}

void Session::removeComponent(Index component_id)
{
    model_.removeComponent(component_id);
}

void Session::boundaryWrite(Index component_id, std::string label, const std::function<void(ComponentOperator&)>& write)
{
    // 操作边界：undo 自动记录 + 通知统一 flush；异常时先提交（部分写入可撤销）再 flush 重抛
    undo_stack_->beginOperation(std::move(label));
    try {
        if (auto op = model_.getComponentOperator(component_id))
            write(*op);
    } catch (...) {
        undo_stack_->commitOperation();
        model_.flushNotifications();
        throw;
    }
    undo_stack_->commitOperation();
    model_.flushNotifications();
}

void Session::removeMesh(Index component_id)
{
    boundaryWrite(component_id, "移除网格", [](ComponentOperator& op) { op.removeMesh(); });
}

void Session::removeGeometry(Index component_id)
{
    boundaryWrite(component_id, "移除几何", [](ComponentOperator& op) { op.removeGeometry(); });
}

void Session::teardown()
{
    if (torn_down_)
        return;
    torn_down_ = true;
    // 1) 先停功能系统：此刻 UndoStack / 观察者链路均存活，deactivate 内的 staged 清理安全
    feature_system_.reset();
    // 2) 断开栈与模型层的互指钩子：此后成员按任意顺序析构都不会回触已析构对象
    undo_stack_->setOnChanged(nullptr);
    model_.setUndoRecorder(nullptr);
}
}
