#pragma once
// ModelImporter.h

#include <optional>            // for std::optional
#include <QUrl>
#include <QString>
#include "ModelOperator.h"
#include "ModelManager.h"

/**
 * @brief 用于导入模型，暴露给UI的模型层操作器
 */
class ModelImporter {
public:
    explicit ModelImporter(ModelManager& mgr) : mgr_(mgr) {}
    std::optional<ModelOperator> import(const std::filesystem::path& url);   // 自动分发
private:
    ModelManager& mgr_;
};