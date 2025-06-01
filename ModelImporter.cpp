// ModelImporter.cpp
#include "ModelImporter.h"      // 声明 ModelImporter
#include "FileHandler.h"        // 调用 readMesh/readSpline
#include <QFileInfo>            // 用来取后缀
#include <optional>             // for std::nullopt and std::optional
std::optional<ModelOperator> ModelImporter::import(const QUrl & url) {
    const QString ext = QFileInfo(url.toLocalFile()).suffix().toLower();
    std::unique_ptr<ModelData> m;
    if (ext == "obj" || ext == "stl" || ext == "ply")
        m = FileHandler::instance().readMesh(url);
    else if (ext == "step" || ext == "stp" || ext == "iges" || ext == "igs")
        m = FileHandler::instance().readSpline(url);
    else {
        qWarning() << "import: unsupported" << ext;
        return std::nullopt;
    }
    if (!m) return std::nullopt;
    Index id = mgr_.addModel(std::move(m));
    return mgr_.getModelOperator(id);
}