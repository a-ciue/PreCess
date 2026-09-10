/**
 * @file ModelIOSystem.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "ModelIOSystem.h"
#include "ModelIOHandler.h"
#include "ModelIOInfo.h"
#include "ModelLayer.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <spdlog/fmt/ranges.h>
#include <spdlog/spdlog.h>

namespace systems::io {
using std::string;
using std::unique_ptr;
using std::vector;

namespace {
    //! @brief 忽略大小写比较两个扩展名（均不含前置点）
    bool equalsIgnoreCase(const string& lhs, const string& rhs)
    {
        return lhs.size() == rhs.size()
            && std::equal(lhs.begin(), lhs.end(), rhs.begin(), [](unsigned char a, unsigned char b) {
                   return std::tolower(a) == std::tolower(b);
               });
    }
}

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

string ModelIOSystem::preferredExtension(const string& file_type) const
{
    const auto info_it = file_type_infos_.find(file_type);
    if (info_it == file_type_infos_.end() || info_it->second->extensions.empty())
        return {};
    return info_it->second->extensions.front();
}

bool ModelIOSystem::isRegisteredExtension(const string& extension) const
{
    if (extension.empty())
        return false;

    // 注册类型为个位数，线性扫描即可（与读写侧的文件类型解析策略一致）
    for (const auto& entry : file_type_infos_) {
        for (const auto& registered : entry.second->extensions) {
            if (equalsIgnoreCase(extension, registered))
                return true;
        }
    }
    return false;
}

string ModelIOSystem::suggestFileName(const string& model_name, const string& file_type) const
{
    const string target_extension = preferredExtension(file_type);
    // 目标类型未知时无法给出扩展名，原样返回：模型名自带的扩展名是反查文件类型的唯一线索
    if (model_name.empty() || target_extension.empty())
        return model_name;

    // 只剥掉"看起来是已注册扩展名"的后缀，普通带点名字（如 "part.v2"）保持完整
    const auto dot_pos = model_name.rfind('.');
    const bool has_suffix = dot_pos != string::npos && dot_pos > 0 && dot_pos + 1 < model_name.size();
    const string base_name = has_suffix && isRegisteredExtension(model_name.substr(dot_pos + 1))
        ? model_name.substr(0, dot_pos)
        : model_name;

    return base_name + "." + target_extension;
}

std::filesystem::path ModelIOSystem::adaptFileExtension(const std::filesystem::path& path, const string& file_type) const
{
    const string target_extension = preferredExtension(file_type);
    if (path.empty() || target_extension.empty())
        return path;

    string current_extension = path.extension().string();
    if (!current_extension.empty() && current_extension.front() == '.')
        current_extension.erase(current_extension.begin());

    if (equalsIgnoreCase(current_extension, target_extension))
        return path;
    // 非空且未注册的扩展名视为用户显式指定（如 "out.dat"），不擅自改写
    if (!current_extension.empty() && !isRegisteredExtension(current_extension))
        return path;

    std::filesystem::path adapted = path;
    adapted.replace_extension(target_extension);
    return adapted;
}
}
