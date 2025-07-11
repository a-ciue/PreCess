#include <spdlog/spdlog.h>
#include <spdlog/fmt/ranges.h>
#include <optional>

#include "ModelIOSystem.h"
#include "ModelIOHandler.h"
#include "../../ModelManager.h"

namespace systems::io {
ModelIOSystem::ModelIOSystem(ModelManager& manager)
    : manager_(&manager)
{
}

void ModelIOSystem::read(const std::filesystem::path& path, const string& file_type, const std::vector<std::any>& args)
{
    // 检查文件类型是否已注册
    ModelIOHandler* handler = this->handlers_.count(file_type) ? this->handlers_[file_type].get() : nullptr;
    if (!handler) {
        spdlog::warn(u8"写文件时没有注册文件类型 {}", file_type);
        return;
    }

    unique_ptr<ModelData> data = this->handlers_[file_type]->read_model(path, args);
    this->manager_->addModel(move(data));
}

void ModelIOSystem::write(Index model, const std::filesystem::path& path, const string& file_type, const std::vector<std::any>& args)
{
    // 检查文件类型是否已注册
    ModelIOHandler* handler = this->handlers_.count(file_type) ? this->handlers_[file_type].get() : nullptr;
    if (!handler) {
        spdlog::warn("写文件时没有注册文件类型 {}", file_type);
        return;
    }

    if (ModelData* model_data = this->manager_->getModel(model)) {
        handler->write_model(*model_data, path, args);
    } else {
        spdlog::warn("模型 id {} 不存在，无法写出", model);
    }
}

void ModelIOSystem::registerHandler(std::unique_ptr<ModelIOHandler> handler)
{
    if (!handler) {
        spdlog::warn(u8"没有传入有效handler");
        return;
    }

    ModelIOHandler& handlerRef = *handler;
    string file_type = handlerRef.file_type();
    this->handlers_[file_type] = move(handler);
    this->fileExtensions_[file_type] = this->handlers_[file_type]->file_extensions();

    spdlog::info("注册文件类型 {}，支持扩展名：{}", file_type, fmt::join(this->handlers_[file_type]->file_extensions(), ", "));
}
}
