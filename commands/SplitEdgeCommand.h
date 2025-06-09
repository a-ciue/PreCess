//
// Created by 张家僮 on 5/14/25.
//
#pragma once
#include "ICommand.h"
#include "../ModelOperator.h"
#include "ArgTypeObject.h"
#include "../ModelImporter.h"
class QSelection;

/**
 * SplitFaceCommand：拆分选中面的命令
 */
class SplitEdgeCommand : public ICommand {
public:
    SplitEdgeCommand(ModelOperator model_op, QSelection* selection);
    void execute() override;
    void undo() override;
    void redo() override;

    static QList<ArgTypeObject*> getArgsModel();
    static unique_ptr<SplitEdgeCommand> create(ModelOperator model_op, ModelImporter& importer, const QVariantList& list);

private:
    ModelOperator model_op_;
    QSelection* selection_;
};
