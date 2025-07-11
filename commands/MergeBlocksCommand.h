//
// Created by 徐昊阳 on 4/12/25.
//
// command/MergeBlocksCommand.h
#pragma once
#include "ICommand.h"
#include "../ModelOperator.h"
#include "QArgObject.h"
#include "../ModelImporter.h"
class QSelection;

/**
 * MergeBlocksCommand：将多个块合并到第一个块的命令
 */
class MergeBlocksCommand : public ICommand {
public:
    MergeBlocksCommand(ModelOperator model_op, QSelection* selection);
    void execute() override;
    void undo() override;
    void redo() override;

    static QList<QArgObject*> getArgsModel();
    static std::unique_ptr<MergeBlocksCommand> create(ModelOperator model_op, ModelImporter& importer, const QVariantList& list);
private:
    ModelOperator model_op_;
    QSelection* selection_;
};
