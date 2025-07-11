//
// Created by 张家僮 on 6/21/25.
//
#pragma once
#include "ICommand.h"
#include "../ModelOperator.h"
#include "ArgTypeObject.h"
#include "../ModelImporter.h"
#include <filesystem>
class QSelection;

/**
* @brief 命令：执行输入命令，并在执行完毕后读入网格
*/
class CmdExecuteCommand : public ICommand {
public:
    CmdExecuteCommand(ModelOperator model_op, ModelImporter& importer, const std::filesystem::path& cmd, const std::string& args);
    void execute() override;
    void undo() override;
    void redo() override;

    static QList<ArgTypeObject*> getArgsModel();
    static unique_ptr<CmdExecuteCommand> create(ModelOperator model_op, ModelImporter& importer, const QVariantList& list);

private:
    ModelOperator model_op_;
    ModelImporter& importer_;
    std::filesystem::path cmd_ {};
    std::string args_ {};
};
