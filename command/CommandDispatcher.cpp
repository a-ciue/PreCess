//
// Created by 徐昊阳 on 4/12/25.
// 修改后：CommandDispatcher 通过 ModelBridge 获取 ModelData，而不直接使用 ModelManager
//

#include "CommandDispatcher.h"
#include "SplitFaceCommand.h"
#include "MergeBlocksCommand.h"

#include "LoadSplineCommand.h"
#include "Selection.h"
#include <QDebug>

CommandDispatcher::CommandDispatcher(QObject* parent)
        : QObject(parent) { }

ModelBridge* CommandDispatcher::modelBridge() const {
    return m_modelBridge;
}

void CommandDispatcher::setModelBridge(ModelBridge* bridge) {
    m_modelBridge = bridge;
    emit modelChanged();
}

void CommandDispatcher::splitFace(const QString& modelName, QSelection* sel) {
    if (!m_modelBridge) {
        qWarning() << "未设置 ModelBridge";
        return;
    }
    // 通过 ModelBridge 获取对应的 ModelData
    auto dataPtr = m_modelBridge->getData();
    if (!dataPtr) {
        qWarning() << "模型数据为空";
        return;
    }
    // 使用 ModelData 指针传递给具体命令类
    auto cmd = std::make_unique<SplitFaceCommand>(dataPtr.get(), sel);
    cmd->execute();
    m_history.push_back(std::move(cmd));
}

void CommandDispatcher::mergeBlocks(const QString& modelName, QSelection* sel) {
    if (!m_modelBridge) {
        qWarning() << "未设置 ModelBridge";
        return;
    }
    auto dataPtr = m_modelBridge->getData();
    if (!dataPtr) {
        qWarning() << "模型数据为空";
        return;
    }
    auto cmd = std::make_unique<MergeBlocksCommand>(dataPtr.get(), sel);
    cmd->execute();
    m_history.push_back(std::move(cmd));
}

void CommandDispatcher::loadSpline(const QUrl& path) {
    if (!m_modelBridge) {
        qWarning() << "未设置 ModelBridge";
        return;
    }
    // 此处假设 loadSpline 命令需要在 ModelBridge 管理下创建新 ModelData
    auto dataPtr = m_modelBridge->getData();
    auto cmd = std::make_unique<LoadSplineCommand>(dataPtr.get(), path);
    cmd->execute();
    m_history.push_back(std::move(cmd));
}

void CommandDispatcher::undo() {
    if (m_history.empty()) {
        qWarning() << "无可撤销命令";
        return;
    }
    auto cmd = std::move(m_history.back());
    m_history.pop_back();
    cmd->undo();
}
