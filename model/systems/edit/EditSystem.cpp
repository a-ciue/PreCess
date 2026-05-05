/**
 * @file EditSystem.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "EditSystem.h"
#include "ArgObject.h"
#include "EditHandler.h"
#include "ModelLayer.h"

#include <spdlog/spdlog.h>

namespace systems::edit {
using core::ArgObject;
using std::string;
using std::vector;

const string EditSystem::name = "EditSystem";

EditSystem::EditSystem(ModelLayer& model_manager)
    : model_manager_(&model_manager)
{
    on_edit_info_changed_ = []() { };
}

EditSystem::~EditSystem() = default;

std::any EditSystem::call(const string& unique_name, Index component_id, const vector<ArgObject>& args)
{
    std::optional comp_op = model_manager_->getComponentOperator(component_id);
    if (!comp_op) {
        spdlog::error("EditSystem::call: ComponentData operator for component ID {} not found.", component_id);
        return {};
    }
    auto it = handlers_.find(unique_name);
    if (it != handlers_.end() && it->second) {
        // 2) 调用插件 execute，传入 ComponentOperator
        std::any result = it->second->execute(*comp_op, args);

        // 3) 通知变更（ComponentOperator 内已封装 notifyChanged）
        comp_op->notifyChanged();

        return result;
    }

    spdlog::warn("EditSystem::call: Handler '{}' not found.", unique_name);
    return {};
}

bool EditSystem::registerHandler(const HandlerMetaData& meta_data, SystemHandlerPtr handler)
{
    if (!handler)
        return false;

    auto info = std::make_unique<EditInfo>();
    info->name = meta_data.name;
    info->display_name = meta_data.display_name;
    info->arg_types = handler->args_type();
    this->edit_infos_[meta_data.name] = std::move(info);

    handlers_[meta_data.name] = std::move(handler);
    on_edit_info_changed_();
    spdlog::info("EditSystem::registerHandler: Registered handler for edit '{}'", meta_data.name);
    return true;
}

void EditSystem::unregisterHandler(const HandlerMetaData& meta_data)
{
    if (handlers_.count(meta_data.name) == 0) {
        spdlog::warn("EditSystem::unregisterHandler: Handler for edit '{}' not found", meta_data.name);
    }

    handlers_.erase(meta_data.name);
    this->edit_infos_.erase(meta_data.name);
    on_edit_info_changed_();

    spdlog::info("EditSystem::unregisterHandler: Unregistered handler for edit '{}'", meta_data.name);
}

vector<EditInfo*> EditSystem::getEditInfos()
{
    vector<EditInfo*> infos;
    infos.reserve(edit_infos_.size());
    for (auto&& [algo_name, algo_info] : edit_infos_) {
        infos.push_back(algo_info.get());
    }
    return infos;
}

std::optional<std::vector<core::ArgType>> EditSystem::getArgTypes(const std::string& unique_name)
{
    auto it = handlers_.find(unique_name);
    if (it != handlers_.end() && it->second) {
        return it->second->args_type();
    }
    return {};
}

void EditSystem::setOnEditInfoChangedCallback(std::function<void()> callback)
{
    on_edit_info_changed_ = std::move(callback);
}
}
