// ModelImporter.cpp
#include "ModelImporter.h"      // 声明 ModelImporter
#include "FileHandler.h"        // 调用 readMesh/readSpline
#include <QFileInfo>            // 用来取后缀
#include <optional>             // for std::nullopt and std::optional
std::optional<ModelOperator> ModelImporter::import(const std::filesystem::path& path) {
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