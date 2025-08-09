//
// Created by 张家僮 on 7/4/25.
//
#pragma once
#include "ICommand.h"
#include "../ModelOperator.h"
#include "ArgTypeObject.h"
#include "../ModelImporter.h"
class QSelection;

/**
 * SplitEdgeCommand：拆分选中面的命令。根据给定id找到mesh的edge，进行边分割
 */
class RemeshBlockCommand : public ICommand {
public:
    RemeshBlockCommand(ModelOperator model_op, QSelection* selection);
    void execute() override;
    void undo() override;
    void redo() override;

    static QList<ArgTypeObject*> getArgsModel();
    static unique_ptr<RemeshBlockCommand> create(ModelOperator model_op, ModelImporter& importer, const QVariantList& list);

private:
    ModelOperator model_op_;
    QSelection* selection_;
};
