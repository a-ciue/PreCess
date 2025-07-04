//
// Created by 徐昊阳 on 4/12/25.
// 修改后：CommandDispatcher 通过 ModelBridge 获取 ModelData，而不直接使用 ModelManager
//

#include "CommandDispatcher.h"
#include "MergeBlocksCommand.h"

#include "LoadSplineCommand.h"
#include "../Selection.h"
#include <QDebug>

#include "../ModelManager.h"

CommandDispatcher::CommandDispatcher(ModelManager* manager, QObject* parent)
        : QObject(parent), model_manager_(manager), importer_(*manager) { }

void CommandDispatcher::runCommand(QCommand* cmd, Index model_id, const QVariantList& args)
{
    if (!model_manager_) {
        qWarning() << "未设置 ModelManager";
        return;
    }
    if (!cmd) {
        qWarning() << "未设置 QCommand";
        return;
    }

    // 通过 ModelBridge 获取对应的 ModelData
    std::optional model_operator = model_manager_->getModelOperator(model_id);
    if (!model_operator) {
        qWarning() << "未找到模型: " << model_id;
        return;
    }
    // 使用 ModelData 指针传递给具体命令类
    if (auto command = cmd->create(*model_operator, importer_,args))
    {
        command->execute();
        m_history.push_back(std::move(command));
    }
    else
    {
        qWarning() << "命令" << cmd->name() << "创建失败";
    }
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
