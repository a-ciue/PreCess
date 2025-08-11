#include "QModelManager.h"
#include "AlgorithmSystemRegister.h"
#include "ModelIOSystemRegister.h"
#include "ModelManager.h"
#include "ModelIOSystem.h"
#include "AlgorithmSystem.h"
#include "SystemPluginManager.h"
#include "QModelObserver.h"

#include <QDebug>
#include <filesystem>
#include <QFileInfo>


QModelManager::QModelManager(QObject* parent)
    : QObject(parent)
{
    // 1) 初始化
    observer_ = std::make_unique<QModelObserver>();
    core_ = std::make_unique<ModelManager>(
        /*observer=*/observer_.get()
    );

    io_system_ = std::make_unique<systems::io::ModelIOSystem>(*core_);
    algo_system_ = std::make_unique<systems::algo::AlgorithmSystem>(*io_system_, *core_);
    plugin_manager_ = std::make_unique<systems::SystemPluginManager>();

    // 2) 注册系统
    plugin_manager_->addSystemRegister(systems::io::ModelIOSystem::name, std::make_unique<systems::io::ModelIOSystemRegister>(*io_system_));
    plugin_manager_->addSystemRegister(systems::algo::AlgorithmSystem::name, std::make_unique<systems::algo::AlgorithmSystemRegister>(*algo_system_));

    // 3) 注册插件
    using std::filesystem::path;
    path plugin_dir = std::filesystem::current_path() / "../plugins";
    // 遍历插件目录，加载所有插件
    for (const auto& entry : std::filesystem::directory_iterator(plugin_dir)) {
        if (entry.is_regular_file()) {
            plugin_manager_->registerPlugin(entry.path());
        }
    }
}

QModelManager::~QModelManager() = default;

void QModelManager::importModel(const QUrl& url)
{
    // 尝试对obj模型走 IOSystem 读取
    if (QString ext = QFileInfo(url.toLocalFile()).suffix().toLower();
        ext == "obj") {
        io_system_->read(url.toLocalFile().toStdString(), "Wavefront .obj file", {});
    }
    else {
        qWarning() << "QModelManager::importModel 导入失败: " << url;
    }
}

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
