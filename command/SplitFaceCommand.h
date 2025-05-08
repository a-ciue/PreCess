//
// Created by 徐昊阳 on 4/12/25.
//
// command/SplitFaceCommand.h
#pragma once
#include "ICommand.h"
class ModelData;
class QSelection;

/**
 * SplitFaceCommand：拆分选中面的命令
 */
class SplitFaceCommand : public ICommand {
public:
    SplitFaceCommand(ModelData* model, QSelection* selection);
    void execute() override;
    void undo() override;
private:
    ModelData* model_;
    QSelection* selection_;
};
