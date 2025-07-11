//
// Created by 张家僮 on 6/21/25.
//
#include "CmdExecuteCommand.h"
#include <QVariantList>
#include <filesystem>
#include "../ModelImporter.h"

CmdExecuteCommand::CmdExecuteCommand(ModelOperator model_op, ModelImporter& importer, const std::filesystem::path& cmd, const std::string& args)
    : model_op_(model_op), importer_(importer), cmd_(cmd), args_(args) { }

void CmdExecuteCommand::execute()
{
    using std::filesystem::path;
	path exe_dir = cmd_;
	exe_dir.remove_filename();

    string input_file {};
    if (auto mesh_op = model_op_.data()->asMeshData()) {
        input_file = "input.obj";
        model_op_.write_mesh(exe_dir / input_file, ModelRenderMode::Face, "obj");
    } else {
        input_file = "input.stp";
        model_op_.write_spline(exe_dir / input_file);
    }

    std::string full_command { cmd_.string() + " " + input_file + " " + args_ };
    std::cout << "Executing command: " << full_command << std::endl;
    std::system(full_command.c_str());

    path output_dir;
    if (std::filesystem::exists(output_dir = exe_dir / "output.obj") 
        || std::filesystem::exists(output_dir = exe_dir / "output.stp")) {
        importer_.import(output_dir);
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
    model << new ArgTypeObject(0, "功能(.exe,.bat)", "")
          << new ArgTypeObject(4, "参数列表", "");

    return model;
}

unique_ptr<CmdExecuteCommand> CmdExecuteCommand::create(ModelOperator model_op, ModelImporter& importer, const QVariantList& list)
{
    // 根据传入的参数创建 SplitFaceCommand 对象

    const QString& cmd_origin = list.at(0).value<QString>();
    const std::filesystem::path& cmd = QFileInfo(cmd_origin).filesystemFilePath();
    const std::string& args = list.at(1).value<QString>().toStdString();

    return std::make_unique<CmdExecuteCommand>(model_op, importer, cmd, args);
}
