// ModelImporter.cpp
#include "ModelImporter.h"      // 声明 ModelImporter
#include "FileHandler.h"        // 调用 readMesh/readSpline
#include "CTMeshModel.h"
#include <QFileInfo>            // 用来取后缀
#include <optional>             // for std::nullopt and std::optional
std::optional<ModelOperator> ModelImporter::import(const std::filesystem::path& path) {
    // TODO: 暂时在这测试CTMeshModel
    auto data = std::make_unique<MeshData>();
    CTMeshModel model(path);
    model.update(*data);
    data->model_name_ = QFileInfo(QString::fromStdString(path.string())).baseName();

    auto m = std::make_unique<ModelData>(std::move(*data));
    Index id = mgr_.addModel(std::move(m));
    return mgr_.getModelOperator(id);
}