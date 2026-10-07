/**
 * @file ModelIOSystem.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "ModelIOSystem.h"
#include "ModelIOHandler.h"
#include "ModelLayer.h"
#include "ModelScope.h"

#include <optional>
#include <spdlog/fmt/ranges.h>
#include <spdlog/spdlog.h>
#include <utility>

namespace systems::io {
using std::string;
using std::vector;

const string ModelIOSystem::name = "ModelIOSystem";

ModelIOSystem::ModelIOSystem(ModelLayer& manager, UndoStack* stack)
    : manager_(&manager)
    , undo_stack_(stack)
{
    on_dialog_name_filters_changed_ = []() { };
}

ModelIOSystem::~ModelIOSystem() = default;

bool ModelIOSystem::read(const std::filesystem::path& path, const string& file_type, const std::vector<std::any>& args)
{
    ModelScope scope(*manager_, undo_stack_, "添加模型");
    auto payload = parseModel(path, file_type, args);
    if (!payload)
        return false;

    this->manager_->addModel(u8Narrow(payload->model_name), std::move(payload->components));
    return true;
}

std::optional<ModelPayload> ModelIOSystem::parseModel(const std::filesystem::path& path, const string& file_type,
    const std::vector<std::any>& args)
{
    // 检查文件类型是否已注册
    auto it = entries_.find(file_type);
    if (it == entries_.end()) {
        spdlog::error(R"(file type "{}" not registered when read model file)", file_type);
        return std::nullopt;
    }

    auto payload = it->second.handler->read_model(path, args);
    if (!payload) {
        spdlog::error(R"(failed to read model from file "{}" as file type "{}")", path.string(), file_type);
        return std::nullopt;
    }
    return payload;
}

void ModelIOSystem::write(Index model, const std::filesystem::path& path, const string& file_type, const std::vector<std::any>& args)
{
    // 检查文件类型是否已注册
    auto it = entries_.find(file_type);
    if (it == entries_.end()) {
        spdlog::warn("file type {} not registered when write model file", file_type);
        return;
    }

    auto* m = manager_->modelById(model);
    auto cids = m ? m->componentIds() : std::vector<Index> { };
    if (cids.empty()) {
        spdlog::warn("ModelIOSystem::write: model {} has no components", model);
        return;
    }

    it->second.handler->write_components(*manager_, cids, path, args);
}

void ModelIOSystem::writeComponents(const std::vector<Index>& component_ids,
    const std::filesystem::path& path,
    const std::string& file_type,
    const std::vector<std::any>& args)
{
    auto it = entries_.find(file_type);
    if (it == entries_.end()) {
        spdlog::warn("file type {} not registered when write model file", file_type);
        return;
    }

    it->second.handler->write_components(*manager_, component_ids, path, args);
}

bool ModelIOSystem::registerHandler(const HandlerMetaData& meta_data, SystemHandlerPtr handler)
{
    manager_->assertOperationIdle();
    const string& file_type = meta_data.file_type;
    if (!handler || entries_.contains(file_type)) {
        // 保持重复格式拒绝；不访问无效或被拒处理器的元数据。
        return false;
    }

    ModelIOInfo info { file_type,
        "", // TODO: 以后从meta_data中获取描述信息
        meta_data.extensions,
        handler->read_args_type(),
        handler->write_args_type() };
    entries_.emplace(file_type, IOEntry { std::move(handler), std::move(info) });

    spdlog::info("registered file type: {}, supported file extension: {}", file_type, fmt::join(meta_data.extensions, ", "));
    on_dialog_name_filters_changed_();

    return true;
}

void ModelIOSystem::unregisterHandler(const HandlerMetaData& meta_data)
{
    manager_->assertOperationIdle();
    const string& file_type = meta_data.file_type;
    entries_.erase(file_type);

    spdlog::info("unregistered file type: {}", file_type);
    on_dialog_name_filters_changed_();
}

std::vector<ModelIOInfo*> ModelIOSystem::registeredFileTypeInfos()
{
    vector<ModelIOInfo*> infos;
    infos.reserve(entries_.size());
    for (auto&& [file_type, entry] : entries_) {
        infos.push_back(&entry.info);
    }

    return infos;
}

void ModelIOSystem::setOnDialogNameFiltersChanged(std::function<void()> callback)
{
    on_dialog_name_filters_changed_ = std::move(callback);
}
}
