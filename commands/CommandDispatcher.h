//
// Created by 徐昊阳 on 4/12/25.
// 修改后：CommandDispatcher 不再依赖 ModelManager，而是依赖 ModelBridge 作为数据上下文注入
//
#pragma once
#include <QObject>
#include <qqmlintegration.h>
#include <QUrl>
#include <vector>
#include <memory>
#include "ICommand.h"
#include "QCommand.h"
#include <QVariant>

#include "../ModelImporter.h"
#include "../ModelManager.h"

class QSelection;

/**
 * CommandDispatcher：命令调度器，负责命令的执行
 * 通过 ModelBridge 获取数据上下文(ModelData)供各命令调用，
 * 数据操作逻辑全部在各具体命令类中实现
 */
class CommandDispatcher : public QObject {
	Q_OBJECT
    QML_ELEMENT
public:
    explicit CommandDispatcher(ModelManager* manager, QObject* parent = nullptr);

    Q_INVOKABLE void runCommand(QCommand* cmd, Index model_id, const QVariantList& args);
    //Q_INVOKABLE void mergeBlocks(const QString& model_name, QSelection* sel);
    //Q_INVOKABLE void renameModel(const QString& oldName, const QString& newName);
    //Q_INVOKABLE void loadSpline(const QUrl& path);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

signals:

private:
    // 修改：使用 ModelBridge 代替原来 ModelManager
    ModelManager* model_manager_{};
    std::vector<std::unique_ptr<ICommand>> m_history;
    ModelImporter importer_;
};
