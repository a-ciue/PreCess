//
// Created by 张家僮 on 5/14/25.
//
// command/TrianglulationCommand.cpp
#include "TrianglulationCommand.h"
#include <QVariantList>
#include <filesystem>
#include "../ModelImporter.h"
#include "../ToolMesh.h"

TrianglulationCommand::TrianglulationCommand(ModelOperator model_op, ModelImporter& importer)
        : model_op_(model_op),importer_(importer) { }

void TrianglulationCommand::execute() {
    std::filesystem::remove_all("./Data/PatchedMesh");
    std::filesystem::create_directories("./Data/PatchedMesh");
    model_op_.write_spline(".\\Data\\PatchedMesh\\temp.stp");

    // cmd = Spline2Tri_BaseGen_Command.bat spline_dir output 60
    std::string cmd =
        "Spline2Tri_BaseGen_Command.bat " ".\\Data\\PatchedMesh\\temp.stp" " .\\Data\\PatchedMesh\\temp 60";
    std::system(cmd.c_str());

    std::string stitch_cmd = ".\\Bin\\MeshStitching.exe .\\Data\\PatchedMesh\\ "
        ".\\Data\\PatchedMesh\\temp_BadPatches.txt "
        ".\\Data\\temp.m";
    std::system(stitch_cmd.c_str());

    std::filesystem::path output_dir = "./Data/temp.m";

    std::unique_ptr<MeshLib::CTMesh> mesh = std::make_unique<MeshLib::CTMesh>();
    mesh->read_m(output_dir.string().c_str());
    
    importer_.import(output_dir.string());
}

void TrianglulationCommand::undo() {
    // TODO: 实现撤销逻辑（例如删除新顶点、恢复原面结构）
}

void TrianglulationCommand::redo()
{
}

QList<QArgObject*> TrianglulationCommand::getArgsModel()
{
    QList<QArgObject*> model;

    return model;
}

std::unique_ptr<TrianglulationCommand> TrianglulationCommand::create(ModelOperator model_op, ModelImporter& importer, const QVariantList& list)
{
    // 根据传入的参数创建 SplitFaceCommand 对象
    return std::make_unique<TrianglulationCommand>(model_op, importer);
}
