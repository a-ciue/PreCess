#include "QModelManager.h"
#include "AlgorithmSystem.h"
#include "AlgorithmSystemRegister.h"
#include "EditSystem.h"
#include "EditSystemRegister.h"
#include "ModelIOSystem.h"
#include "ModelIOSystemRegister.h"
#include "ModelManager.h"
#include "QModelObserver.h"
#include "SystemPluginManager.h"


#include <QDebug>
#include <QFileInfo>
#include <filesystem>
#include <spdlog/spdlog.h>

QModelManager::QModelManager(std::string_view argv0, QObject* parent)
    : QObject(parent)
{
    // 1) 初始化
    observer_ = std::make_unique<QModelObserver>();
    core_ = std::make_unique<ModelManager>(
        /*observer=*/observer_.get());

    io_system_ = std::make_unique<systems::io::ModelIOSystem>(*core_);
    algo_system_ = std::make_unique<systems::algo::AlgorithmSystem>(*io_system_, *core_);
    edit_system_ = std::make_unique<systems::edit::EditSystem>(*core_);
    plugin_manager_ = std::make_unique<systems::SystemPluginManager>();

    // 2) 注册系统
    plugin_manager_->addSystemRegister(systems::io::ModelIOSystem::name, std::make_unique<systems::io::ModelIOSystemRegister>(*io_system_));
    plugin_manager_->addSystemRegister(systems::algo::AlgorithmSystem::name, std::make_unique<systems::algo::AlgorithmSystemRegister>(*algo_system_));
    plugin_manager_->addSystemRegister(systems::edit::EditSystem::name, std::make_unique<systems::edit::EditSystemRegister>(*edit_system_));

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
            // plugin_manager_->registerPlugin(entry.path());
            getSystemPluginManager()->registerPlugin(QString::fromStdString(entry.path().string()));
        }
    }
}

QModelManager::~QModelManager() = default;

void QModelManager::removeModel(int id)
{
    core_->removeModel(id);
    emit modelRemoved(id);
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

ModelManager* QModelManager::getModelManager()
{
    return core_.get();
}

QModelObserver* QModelManager::getModelObserver()
{
    return observer_.get();
}

systems::algo::QAlgorithmSystemAdaptor QModelManager::getAlgorithmSystemAdaptor()
{
    return systems::algo::QAlgorithmSystemAdaptor(*algo_system_);
}

systems::edit::QEditSystemAdaptor QModelManager::getEditSystemAdaptor()
{
    return systems::edit::QEditSystemAdaptor(*edit_system_);
}

systems::io::QModelIOSystemAdaptor QModelManager::getModelIOSystemAdaptor()
{
    return systems::io::QModelIOSystemAdaptor(*io_system_);
}

systems::QSystemPluginManager* QModelManager::getSystemPluginManager()
{
    return q_plugin_manager_.get();
}