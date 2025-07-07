#include "QModelManager.h"
#include "ModelManager.h"
#include "ModelImporter.h"
#include "ModelObserver.h"
#include <filesystem>
// #include "QModelOperatorWrapper.h"   // 若暂不暴露包装器，可注释

QModelManager::QModelManager(QObject* parent)
    : QObject(parent)
{
    // 1) 新建一个 QModelObserver（无参构造）
    observer_ = std::make_unique<QModelObserver>();

    // 2) 用两个参数调用 ModelManager 的构造函数
    core_ = std::make_unique<ModelManager>(
        /*parent=*/nullptr,
        /*observer=*/observer_.get()
    );

    // 3) 新建 ModelImporter，将 core_ 传过去
    importer_ = std::make_unique<ModelImporter>(*core_);
}

void QModelManager::importModel(const QUrl& url)
{
    if (auto maybeOp = importer_->import(std::filesystem::path{ url.toLocalFile().toStdString() })) {
        ModelOperator op = std::move(*maybeOp);
        emit modelAdded(op.getId());
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
