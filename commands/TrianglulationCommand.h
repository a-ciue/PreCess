//
// Created by 张家僮 on 6/2/25.
//
#pragma once
#include "ICommand.h"
#include "ModelOperator.h"
#include "QArgObject.h"
#include "../ModelImporter.h"
class QSelection;

/**
* @brief 命令：进行三角剖分
*/
class TrianglulationCommand : public ICommand {
public:
    TrianglulationCommand(ModelOperator model_op, ModelImporter& importer);
    void execute() override;
    void undo() override;
    void redo() override;

    static QList<QArgObject*> getArgsModel();
    static std::unique_ptr<TrianglulationCommand> create(ModelOperator model_op, ModelImporter& importer, const QVariantList& list);

private:
    ModelOperator model_op_;
    ModelImporter& importer_;
};
