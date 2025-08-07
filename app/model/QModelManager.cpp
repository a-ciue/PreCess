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
    // 1) 新建一个 QModelObserver（无参构造）
    observer_ = std::make_unique<QModelObserver>();

    // 2) 用两个参数调用 ModelManager 的构造函数
    core_ = std::make_unique<ModelManager>(
        /*observer=*/observer_.get()
    );

    io_system_ = std::make_unique<systems::io::ModelIOSystem>(*core_);
    algo_system_ = std::make_unique<systems::algo::AlgorithmSystem>(*io_system_, *core_);

    plugin_manager_ = std::make_unique<systems::SystemPluginManager>();
    plugin_manager_->addSystemRegister(systems::io::ModelIOSystem::name, std::make_unique<systems::io::ModelIOSystemRegister>(*io_system_));
    plugin_manager_->addSystemRegister(systems::algo::AlgorithmSystem::name, std::make_unique<systems::algo::AlgorithmSystemRegister>(*algo_system_));
    plugin_manager_->registerPlugin(R"(D:\proj\Qt\triangulation\out\build\x64-debug\model\plugins\OBJModelPlugin\OBJModelPlugin.dll)");
    plugin_manager_->registerPlugin(R"(D:\proj\Qt\triangulation\out\build\x64-debug\model\plugins\CmdExecutePlugin\CmdExecutePlugin.dll)");
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
