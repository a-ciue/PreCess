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
// 修改：使用 ModelBridge 而非 ModelManager
#include "ModelBridge.h"
#include "QCommand.h"
#include <QVariant>

class QSelection;

/**
 * CommandDispatcher：命令调度器，向 QML 暴露所有命令入口
 * 通过 ModelBridge 获取数据上下文(ModelData)供各命令调用，
 * 数据操作逻辑全部在各具体命令类中实现
 */
class CommandDispatcher : public QObject {
Q_OBJECT
    // 注：这里保留 QML_ELEMENT，如果需要，可改为手动注册
    QML_ELEMENT
    Q_PROPERTY(ModelBridge* modelBridge READ modelBridge WRITE setModelBridge NOTIFY modelBridgeChanged)
public:
    explicit CommandDispatcher(QObject* parent = nullptr);

    ModelBridge* modelBridge() const;
    void setModelBridge(ModelBridge* bridge);

    Q_INVOKABLE void runCommand(QCommand* cmd, const QString& modelName, const QVariantList& args);
    Q_INVOKABLE void splitFace(const QString& modelName, QSelection* sel);
    Q_INVOKABLE void mergeBlocks(const QString& modelName, QSelection* sel);
    //Q_INVOKABLE void renameModel(const QString& oldName, const QString& newName);
    Q_INVOKABLE void loadSpline(const QUrl& path);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

signals:
    //void modelChanged(const QString& modelName);
    void modelChanged();

private:
    // 修改：使用 ModelBridge 代替原来 ModelManager
    ModelBridge* m_modelBridge = nullptr;
    std::vector<std::unique_ptr<ICommand>> m_history;
};
