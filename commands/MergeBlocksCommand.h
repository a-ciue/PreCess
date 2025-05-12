//
// Created by 徐昊阳 on 4/12/25.
//
// command/MergeBlocksCommand.h
#pragma once
#include "ICommand.h"
class vtkModelData;
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
private:
    ModelOperator model_op_;
    QSelection* selection_;
};
