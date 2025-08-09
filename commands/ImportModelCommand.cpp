#include "ImportModelCommand.h"

// 定义 execute，注意这里 **不带 override**，只写函数体
void ImportModelCommand::execute() {
    // 调用 ModelImporter，将文件真正导入，返回一个 ModelOperator
    //op_ = importer_.import(path_);

    const std::filesystem::path ext = path.extension();
	QUrl url = QUrl::fromLocalFile(QString::fromStdU32String(path.u32string()));
    std::unique_ptr<ModelData> m;
    if (ext == ".obj" || ext == ".stl" || ext == ".ply")
        m = FileHandler::instance().readMesh(url);
    else if (ext == ".step" || ext == ".stp" || ext == ".iges" || ext == ".igs")
        m = FileHandler::instance().readSpline(url);
    else if (ext == ".m")
    {
        std::unique_ptr<MeshLib::CTMesh> mesh = std::make_unique<MeshLib::CTMesh>();
        mesh->read_m(url.path().toLatin1());
        MeshData mesh_data{ std::move(mesh) };
		mesh_data.model_name_ = QFileInfo(url.path()).fileName();
        m = std::make_unique<ModelData>(std::move(mesh_data));
    }
    else {
        qWarning() << "import: unsupported" << ext.string();
        return std::nullopt;
    }
    if (!m) return std::nullopt;
    Index id = mgr_.addModel(std::move(m));
    return mgr_.getModelOperator(id);
}

// 空实现
void ImportModelCommand::undo() {
    // 如果未来需要撤销，可以在这里调用 ModelManager::removeModel(...)
}

void ImportModelCommand::redo() {
    execute();
}
