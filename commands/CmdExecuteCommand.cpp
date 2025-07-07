//
// Created by 张家僮 on 6/21/25.
//
#include "CmdExecuteCommand.h"
#include <QVariantList>
#include <filesystem>
#include "../ModelImporter.h"

CmdExecuteCommand::CmdExecuteCommand(ModelImporter& importer, const std::string& cmd, const std::filesystem::path& import_path)
        : importer_(importer),cmd_(cmd),import_path_(import_path) { }

void CmdExecuteCommand::execute()
{
    std::system(cmd_.c_str());

    if (std::filesystem::exists(import_path_))
    {
		importer_.import(import_path_);
    }
}

void CmdExecuteCommand::undo()
{
    // TODO: 实现撤销逻辑（例如删除新顶点、恢复原面结构）
}

void CmdExecuteCommand::redo()
{
}

QList<ArgTypeObject*> CmdExecuteCommand::getArgsModel()
{
    QList<ArgTypeObject*> model;
    model << new ArgTypeObject(4, "cmd命令", "")
          << new ArgTypeObject(0, "结果文件", "./data/output.obj");

    return model;
}

std::unique_ptr<CmdExecuteCommand> CmdExecuteCommand::create(ModelOperator model_op, ModelImporter& importer, const QVariantList& list)
{
    // 根据传入的参数创建 SplitFaceCommand 对象

    const std::string& cmd = list.at(0).value<QString>().toStdString();
    const std::filesystem::path& path = list.at(1).value<QString>().toStdString();

    return std::make_unique<CmdExecuteCommand>(importer, cmd, path);
}
