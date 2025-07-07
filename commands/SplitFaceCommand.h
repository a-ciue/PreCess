//
// Created by 徐昊阳 on 4/12/25.
//
// command/SplitFaceCommand.h
#pragma once
#include "ICommand.h"
#include "../ModelOperator.h"
#include "ArgTypeObject.h"
#include "../ModelImporter.h"
class QSelection;

/**
 * SplitFaceCommand：拆分选中面的命令。根据给定id找到mesh的face，进行面分割
 */
class SplitFaceCommand : public ICommand {
public:
    SplitFaceCommand(ModelOperator model_op, QSelection* selection);
    void execute() override;
    void undo() override;
    void redo() override;

    static QList<ArgTypeObject*> getArgsModel();
    static std::unique_ptr<SplitFaceCommand> create(ModelOperator model_op, ModelImporter& importer, const QVariantList& list);

private:
    ModelOperator model_op_;
    QSelection* selection_;
};
