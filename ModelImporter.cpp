// ModelImporter.cpp
#include "ModelImporter.h"      // 声明 ModelImporter
#include "FileHandler.h"        // 调用 readMesh/readSpline
#include <QFileInfo>            // 用来取后缀
#include <optional>             // for std::nullopt and std::optional
std::optional<ModelOperator> ModelImporter::import(const std::filesystem::path& path) {
    auto data = std::make_unique<MeshData>();
    auto m = std::make_unique<ModelData>(std::move(*data));
    Index id = mgr_.addModel(std::move(m));
    return mgr_.getModelOperator(id);
}