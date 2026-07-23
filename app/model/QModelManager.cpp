#include "QModelManager.h"
#include "AlgorithmSystem.h"
#include "AlgorithmSystemRegister.h"
#include "EditSystem.h"
#include "EditSystemRegister.h"
#include "EventBus.h"
#include "FeatureEvents.h"
#include "FeatureSystem.h"
#include "FeatureSystemRegister.h"
#include "ModelIOSystem.h"
#include "ModelIOSystemRegister.h"
#include "ModelLayer.h"
#include "QModelObserver.h"
#include "SystemPluginManager.h"

#include <QUrl>
#include <filesystem>
#include <spdlog/spdlog.h>

QModelManager::QModelManager(std::string_view argv0, QObject* parent)
    : QObject(parent)
{
    // 1) 初始化
    observer_ = std::make_unique<QModelObserver>();
    core_ = std::make_unique<ModelLayer>(
        /*observer=*/observer_.get());

    query_ = std::make_unique<QModelQuery>(core_.get(), this);

    io_system_ = std::make_unique<systems::io::ModelIOSystem>(*core_);
    algo_system_ = std::make_unique<systems::algo::AlgorithmSystem>(*io_system_, *core_);
    edit_system_ = std::make_unique<systems::edit::EditSystem>(*core_);

    // 功能系统：事件总线 + 系统本体 + 动态上下文 provider 注入
    event_bus_ = std::make_unique<core::EventBus>();
    feature_system_ = std::make_unique<systems::feature::FeatureSystem>(*core_, *event_bus_);

    algo_adaptor_ = std::make_unique<systems::algo::QAlgorithmSystemAdaptor>(*algo_system_);
    io_adaptor_ = std::make_unique<systems::io::QModelIOSystemAdaptor>(*io_system_);
    edit_adaptor_ = std::make_unique<systems::edit::QEditSystemAdaptor>(*edit_system_);
    feature_adaptor_ = std::make_unique<systems::feature::QFeatureSystemAdaptor>(*feature_system_);
    // 功能上下文的活动模型/组件由 UI 同步到适配器，功能经 provider 动态获取
    feature_system_->setActiveModelProvider([this]() { return feature_adaptor_->activeModel(); });
    feature_system_->setActiveComponentProvider([this]() { return feature_adaptor_->activeComponent(); });

    // 参数变更桥接：功能回写参数值（如交互结果）经事件总线转发到 QML 同步显示
    param_bridge_sub_ = event_bus_->subscribe<systems::feature::ParameterChangedEvent>(
        [this](const systems::feature::ParameterChangedEvent& e) {
            feature_adaptor_->notifyParameterChanged(e.feature, e.param_index, e.value);
        });

    // 模型事件桥接到事件总线：功能可订阅 ModelEvent 实时响应模型增删改
    using systems::feature::ModelEvent;
    connect(observer_.get(), &QModelObserver::modelAdded, this, [this](Index id) {
        event_bus_->publish(ModelEvent { ModelEvent::Kind::ModelAdded, id, -1 });
    });
    connect(observer_.get(), &QModelObserver::modelRemoved, this, [this](Index id) {
        event_bus_->publish(ModelEvent { ModelEvent::Kind::ModelRemoved, id, -1 });
    });
    connect(observer_.get(), &QModelObserver::modelChanged, this, [this](Index id) {
        event_bus_->publish(ModelEvent { ModelEvent::Kind::ModelChanged, id, -1 });
    });
    connect(observer_.get(), &QModelObserver::modelNameChanged, this, [this](Index id, const QString&) {
        event_bus_->publish(ModelEvent { ModelEvent::Kind::ModelNameChanged, id, -1 });
    });
    connect(observer_.get(), &QModelObserver::componentChanged, this, [this](Index id) {
        event_bus_->publish(ModelEvent { ModelEvent::Kind::ComponentChanged, -1, id });
    });
    connect(observer_.get(), &QModelObserver::componentRemoved, this, [this](Index id) {
        event_bus_->publish(ModelEvent { ModelEvent::Kind::ComponentRemoved, -1, id });
    });

    plugin_manager_ = std::make_unique<systems::SystemPluginManager>();

    // 2) 注册系统
    plugin_manager_->addSystemRegister(systems::io::ModelIOSystem::name, std::make_unique<systems::io::ModelIOSystemRegister>(*io_system_));
    plugin_manager_->addSystemRegister(systems::algo::AlgorithmSystem::name, std::make_unique<systems::algo::AlgorithmSystemRegister>(*algo_system_));
    plugin_manager_->addSystemRegister(systems::edit::EditSystem::name, std::make_unique<systems::edit::EditSystemRegister>(*edit_system_));
    plugin_manager_->addSystemRegister(systems::feature::FeatureSystem::name, std::make_unique<systems::feature::FeatureSystemRegister>(*feature_system_));

    q_plugin_manager_ = std::make_unique<systems::QSystemPluginManager>(plugin_manager_.get());

    // 3) 注册插件
    using std::filesystem::path;
    path exe_dir = std::filesystem::absolute(argv0).parent_path();
    path plugin_dir = exe_dir / "plugins"; // 对应 开发调试 时的目录结构，相对严格
    if (!std::filesystem::is_directory(plugin_dir)) {
        plugin_dir = exe_dir / "../plugins"; // 对应 install 后的目录结构，相对宽松
    }
    if (!std::filesystem::is_directory(plugin_dir)) {
        spdlog::error("QModelManager::QModelManager: 插件目录 {} 不存在", plugin_dir.string());
        return;
    }
    // 遍历插件目录，加载所有插件
    for (const auto& entry : std::filesystem::directory_iterator(plugin_dir)) {
        if (entry.is_regular_file()) {
            QUrl plugin_url = QUrl::fromLocalFile(QString::fromLocal8Bit(entry.path().string()));
            getSystemPluginManager()->registerPlugin(plugin_url);
        }
    }
}

QModelManager::~QModelManager() = default;

void QModelManager::removeModel(int id)
{
    core_->removeModel(id);
    emit modelRemoved(id);
}

void QModelManager::removeComponent(int id)
{
    core_->removeComponent(id);
}

void QModelManager::removeMesh(int componentId)
{
    auto op = core_->getComponentOperator(componentId);
    if (op)
        op->removeMesh();
}

void QModelManager::removeGeometry(int componentId)
{
    auto op = core_->getComponentOperator(componentId);
    if (op)
        op->removeGeometry();
}

QObject* QModelManager::getOperator(int id)
{
    auto maybeOp = core_->getModelOperator(id);
    if (!maybeOp)
        return nullptr;

    // 暂时不做包装器，直接返回 nullptr
    return nullptr;
    // 如果以后要 QML 操作 ModelOperator，请启用下面这行并实现包装器：
    // return new QModelOperatorWrapper(std::move(*maybeOp), this);
}

ModelLayer* QModelManager::getModelManager()
{
    return core_.get();
}

QModelObserver* QModelManager::getModelObserver() const
{
    return observer_.get();
}

QModelQuery* QModelManager::getModelQuery() const
{
    return query_.get();
}

systems::algo::QAlgorithmSystemAdaptor* QModelManager::getAlgorithmSystemAdaptor() const
{
    return algo_adaptor_.get();
}

systems::edit::QEditSystemAdaptor* QModelManager::getEditSystemAdaptor() const
{
    return edit_adaptor_.get();
}

systems::io::QModelIOSystemAdaptor* QModelManager::getModelIOSystemAdaptor() const
{
    return io_adaptor_.get();
}

systems::feature::QFeatureSystemAdaptor* QModelManager::getFeatureSystemAdaptor() const
{
    return feature_adaptor_.get();
}

systems::QSystemPluginManager* QModelManager::getSystemPluginManager() const
{
    return q_plugin_manager_.get();
}

std::string_view QModelManager::argv0 = "./PreCess.exe";

QModelManager* QModelManager::create(QQmlEngine*, QJSEngine*)
{
    return new QModelManager(argv0);
}