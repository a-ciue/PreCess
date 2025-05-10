//
// Created by 徐昊阳 on 4/12/25.
//
// command/SplitFaceCommand.h
#pragma once
#include "ICommand.h"
#include "ModelOperator.h"
class QSelection;

/**
 * SplitFaceCommand：拆分选中面的命令
 */
class SplitFaceCommand : public ICommand {
public:
    SplitFaceCommand(ModelOperator* model_op, QSelection* selection);
    void execute() override;
    void undo() override;
private:
    ModelOperator* model_op_;
    QSelection* selection_;
};
