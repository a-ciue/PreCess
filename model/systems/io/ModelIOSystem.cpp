/**
 * @file ModelIOSystem.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "ModelIOSystem.h"
#include "ModelIOHandler.h"
#include "ModelManager.h"
#include "ModelIOInfo.h"

#include <spdlog/fmt/ranges.h>
#include <spdlog/spdlog.h>
#include <optional>

namespace systems::io {
using std::string;
using std::unique_ptr;
using std::vector;

const string ModelIOSystem::name = "ModelIOSystem";

ModelIOSystem::ModelIOSystem(ModelManager& manager)
    : manager_(&manager)
{
    on_dialog_name_filters_changed_ = []() { };
}

ModelIOSystem::~ModelIOSystem() = default;

void ModelIOSystem::read(const std::filesystem::path& path, const string& file_type, const std::vector<std::any>& args)
{
    // 检查文件类型是否已注册
    SystemHandler* handler = this->handlers_.count(file_type) ? this->handlers_[file_type].get() : nullptr;
    if (!handler) {
        spdlog::error(R"(file type "{}" not registered when read model file)", file_type);
        return;
    }

    unique_ptr<ModelData> data = handler->read_model(path, args);
    this->manager_->addModel(std::move(data));
}

void ModelIOSystem::write(Index model, const std::filesystem::path& path, const string& file_type, const std::vector<std::any>& args)
{
    // 检查文件类型是否已注册
    SystemHandler* handler = this->handlers_.count(file_type) ? this->handlers_[file_type].get() : nullptr;
    if (!handler) {
        spdlog::warn("file type {} not registered when write model file", file_type);
        return;
    }

    if (std::optional model_op = this->manager_->getModelOperator(model)) {
        handler->write_model(model_op->data(), path, args);
    } else {
        spdlog::warn("model id {} does not exist, cant write model file", model);
    }
}

bool ModelIOSystem::registerHandler(const HandlerMetaData& meta_data, SystemHandlerPtr handler)
{
    string file_type = meta_data.file_type;
    if (this->handlers_.count(file_type)) {
        // 不允许重复注册
        return false;
    }

    auto info = std::make_unique<ModelIOInfo>(ModelIOInfo { file_type,
        "", // TODO: 以后从meta_data中获取描述信息
        meta_data.extensions,
        handler->read_args_type(),
        handler->write_args_type() });
    this->file_type_infos_[file_type] = std::move(info);

    this->handlers_[file_type] = std::move(handler);

    spdlog::info("registered file type: {}, supported file extension: {}", file_type, fmt::join(meta_data.extensions, ", "));
    on_dialog_name_filters_changed_();

    return true;
}

void ModelIOSystem::unregisterHandler(const HandlerMetaData& meta_data)
{
    const string& file_type = meta_data.file_type;
    this->handlers_.erase(file_type);
    this->file_type_infos_.erase(file_type);

    spdlog::info("unregistered file type: {}", file_type);
    on_dialog_name_filters_changed_();
}

std::vector<ModelIOInfo*> ModelIOSystem::registeredFileTypeInfos()
{
    vector<ModelIOInfo*> infos;
    infos.reserve(file_type_infos_.size());
    for (auto&& [algo_name, algo_info] : file_type_infos_) {
        infos.push_back(algo_info.get());
    }

    return infos;
}

void ModelIOSystem::setOnDialogNameFiltersChanged(std::function<void()> callback)
{
    on_dialog_name_filters_changed_ = std::move(callback);
}
}
