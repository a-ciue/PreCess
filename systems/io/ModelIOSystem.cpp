/**
 * @file ModelIOSystem.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ranges.h>
#include <optional>

#include "ModelIOSystem.h"
#include "ModelIOHandler.h"
#include "ModelManager.h"

namespace systems::io {
using std::string;
using std::unique_ptr;
using std::vector;

const string ModelIOSystem::name = "ModelIOSystem";

ModelIOSystem::ModelIOSystem(ModelManager& manager)
    : manager_(&manager)
{
}

void ModelIOSystem::read(const std::filesystem::path& path, const string& file_type, const std::vector<std::any>& args)
{
    // 检查文件类型是否已注册
    ModelIOHandler* handler = this->handlers_.count(file_type) ? this->handlers_[file_type].get() : nullptr;
    if (!handler) {
        spdlog::error(R"(file type "{}" not registered when read model file)", file_type);
        return;
    }

    unique_ptr<ModelData> data = this->handlers_[file_type]->read_model(path, args);
    this->manager_->addModel(std::move(data));
}

void ModelIOSystem::write(Index model, const std::filesystem::path& path, const string& file_type, const std::vector<std::any>& args)
{
    // 检查文件类型是否已注册
    ModelIOHandler* handler = this->handlers_.count(file_type) ? this->handlers_[file_type].get() : nullptr;
    if (!handler) {
        spdlog::warn("file type {} not registered when write model file", file_type);
        return;
    }

    if (ModelData* model_data = this->manager_->getModel(model)) {
        handler->write_model(*model_data, path, args);
    } else {
        spdlog::warn("model id {} does not exist, cant write model file", model);
    }
}

bool ModelIOSystem::registerHandler(const HandlerMetaData& meta_data, std::shared_ptr<ModelIOHandler> handler)
{
    if (!handler) {
        spdlog::warn("{} received an empty handler", __func__);
        return false;
    }

    string file_type = meta_data.file_type;
    if (this->handlers_.count(file_type))
    {
        // 不允许重复注册
        return false;
    }

    this->handlers_[file_type] = handler;
    this->fileExtensions_[file_type] = meta_data.extensions;

    spdlog::info("registered file type: {}, supported file extension: {}", file_type, fmt::join(meta_data.extensions, ", "));
    return true;
}

void ModelIOSystem::unregisterHandler(const HandlerMetaData& meta_data)
{
    const string& file_type = meta_data.file_type;
    this->handlers_.erase(file_type);
    this->fileExtensions_.erase(file_type);
}

const std::unordered_map<string, vector<string>>& ModelIOSystem::registeredFileTypes()
{
    return fileExtensions_;
}
}
