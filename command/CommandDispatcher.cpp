//
// Created by 徐昊阳 on 4/12/25.
// 修改后：CommandDispatcher 通过 ModelBridge 获取 ModelData，而不直接使用 ModelManager
//

#include "CommandDispatcher.h"
#include "SplitFaceCommand.h"
#include "MergeBlocksCommand.h"

#include "LoadSplineCommand.h"
#include "../Selection.h"
#include <QDebug>

#include "../ModelManager.h"

CommandDispatcher::CommandDispatcher(ModelManager* manager, QObject* parent)
        : QObject(parent), model_manager_(manager) { }

void CommandDispatcher::runCommand(QCommand* cmd, const QString& model_name, const QVariantList& args)
{
    if (!model_manager_) {
        qWarning() << "未设置 ModelManager";
        return;
    }
    // 通过 ModelBridge 获取对应的 ModelData
    std::optional model_operator = model_manager_->getModelOperator(model_name);
    if (!model_operator) {
        qWarning() << "未找到模型: " << model_name;
        return;
    }
    // 使用 ModelData 指针传递给具体命令类
    auto command = cmd->create(*model_operator, args);
    if (command)
    {
        command->execute();
        m_history.push_back(std::move(command));
    }
    else
    {
        qWarning() << "命令" << cmd->name() << "创建失败";
    }
}

void CommandDispatcher::splitFace(const QString& model_name, QSelection* sel) {
    if (!model_manager_) {
        qWarning() << "未设置 ModelBridge";
        return;
    }
    // 通过 ModelBridge 获取对应的 ModelData
    std::optional model_operator = model_manager_->getModelOperator(model_name);
    if (!model_operator) {
        qWarning() << "未找到模型: " << model_name;
        return;
    }
    // 使用 ModelData 指针传递给具体命令类
    auto cmd = std::make_unique<SplitFaceCommand>(*model_operator, sel);
    cmd->execute();
    m_history.push_back(std::move(cmd));
}

//void CommandDispatcher::mergeBlocks(const QString& model_name, QSelection* sel) {
//    if (!model_manager_) {
//        qWarning() << "未设置 ModelBridge";
//        return;
//    }
//    std::optional model_operator = model_manager_->getModelOperator(model_name);
//    if (!model_operator) {
//        qWarning() << "未找到模型: " << model_name;
//        return;
//    }
//    auto cmd = std::make_unique<MergeBlocksCommand>(*model_operator, sel);
//    cmd->execute();
//    m_history.push_back(std::move(cmd));
//}
//
//void CommandDispatcher::loadSpline(const QUrl& path) {
//    if (!model_manager_) {
//        qWarning() << "未设置 ModelBridge";
//        return;
//    }
//    // 此处假设 loadSpline 命令需要在 ModelBridge 管理下创建新 ModelData
//    auto dataPtr = model_manager_->getData();
//    auto cmd = std::make_unique<LoadSplineCommand>(dataPtr.get(), path);
//    cmd->execute();
//    m_history.push_back(std::move(cmd));
//}

void CommandDispatcher::undo() {
    if (m_history.empty()) {
        qWarning() << "无可撤销命令";
        return;
    }
    auto cmd = std::move(m_history.back());
    m_history.pop_back();
    cmd->undo();
}

void CommandDispatcher::redo()
{
    if (m_history.empty()) {
        qWarning() << "无可重做命令";
        return;
    }
    auto cmd = std::move(m_history.back());
    m_history.pop_back();
    cmd->execute();
}
