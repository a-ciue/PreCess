// ModelImporter.cpp
#include "ModelImporter.h"      // 声明 ModelImporter
#include "FileHandler.h"        // 调用 readMesh/readSpline
#include "CTMeshModel.h"
#include "MeshData.h"
#include <QFileInfo>            // 用来取后缀
#include <optional>             // for std::nullopt and std::optional
std::optional<ModelOperator> ModelImporter::import(const std::filesystem::path& path) {
    // TODO: 暂时在这测试CTMeshModel
    auto data = std::make_unique<MeshData>();
    CTMeshModel model(path);
    model.update(*data);

    auto m = std::make_unique<ModelData>(std::move(*data));
    m->model_name_ = path.filename().string();
    Index id = mgr_.addModel(std::move(m));
    return mgr_.getModelOperator(id);
}