//
// Created by 张家僮 on 6/21/25.
//
#pragma once
#include "ICommand.h"
#include "ModelOperator.h"
#include "QArgObject.h"
#include "../ModelImporter.h"
#include <filesystem>
class QSelection;

/**
* @brief 命令：执行输入命令，并在执行完毕后读入网格
*/
class CmdExecuteCommand : public ICommand {
public:
    CmdExecuteCommand(ModelImporter& importer, const std::string& cmd, const std::filesystem::path& import_path);
    void execute() override;
    void undo() override;
    void redo() override;

    static QList<QArgObject*> getArgsModel();
    static std::unique_ptr<CmdExecuteCommand> create(ModelOperator model_op, ModelImporter& importer, const QVariantList& list);

private:
    ModelImporter& importer_;
    std::string cmd_ {};
    std::filesystem::path import_path_ {};
};
