/**
 * @file ModelIOSystem.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "ModelIOSystem.h"
#include "ModelIOHandler.h"
#include "ModelIOInfo.h"
#include "ModelLayer.h"

#include <optional>
#include <spdlog/fmt/ranges.h>
#include <spdlog/spdlog.h>

namespace systems::io {
using std::string;
using std::unique_ptr;
using std::vector;

const string ModelIOSystem::name = "ModelIOSystem";

ModelIOSystem::ModelIOSystem(ModelLayer& manager)
    : manager_(&manager)
{
    on_dialog_name_filters_changed_ = []() { };
}

ModelIOSystem::~ModelIOSystem() = default;

bool ModelIOSystem::read(const std::filesystem::path& path, const string& file_type, const std::vector<std::any>& args)
{
    // 检查文件类型是否已注册
    SystemHandler* handler = this->handlers_.count(file_type) ? this->handlers_[file_type].get() : nullptr;
    if (!handler) {
        spdlog::error(R"(file type "{}" not registered when read model file)", file_type);
        return false;
    }

    auto payload = handler->read_model(path, args);
    if (!payload) {
        // 文件内容与该文件类型不符（损坏或选错类型）时无法构造模型，此处返回false而不抛异常：
        // 算法插件读回结果文件时依赖"读取失败不中断"的现有行为，由调用方决定是否中断。
        // 日志经 QtLogSink 进入界面"日志"面板；该面板启动即订阅消息、隐藏时同样累积，
        // 用户打开面板即可看到此处记录的文件路径与文件类型。
        spdlog::error(R"(failed to read model from file "{}" as file type "{}")", path.string(), file_type);
        return false;
    }

    this->manager_->addModel(payload->model_name, std::move(payload->components));
    return true;
}

void ModelIOSystem::write(Index model, const std::filesystem::path& path, const string& file_type, const std::vector<std::any>& args)
{
    // 检查文件类型是否已注册
    SystemHandler* handler = this->handlers_.count(file_type) ? this->handlers_[file_type].get() : nullptr;
    if (!handler) {
        spdlog::warn("file type {} not registered when write model file", file_type);
        return;
    }

    auto* m = manager_->modelById(model);
    auto cids = m ? m->componentIds() : std::vector<Index>{};
    if (cids.empty()) {
        spdlog::warn("ModelIOSystem::write: model {} has no components", model);
        return;
    }

    handler->write_components(*manager_, cids, path, args);
}

void ModelIOSystem::writeComponents(const std::vector<Index>& component_ids,
        const std::filesystem::path& path,
        const std::string& file_type,
        const std::vector<std::any>& args)
{
    SystemHandler* handler = handlers_.count(file_type) ? handlers_[file_type].get() : nullptr;
    if (!handler) {
        spdlog::warn("file type {} not registered when write model file", file_type);
        return;
    }

    handler->write_components(*manager_, component_ids, path, args);
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
