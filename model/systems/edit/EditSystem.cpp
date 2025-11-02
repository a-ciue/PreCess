/**
 * @file EditSystem.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "EditSystem.h"
#include "ArgObject.h"
#include "EditHandler.h"
#include "ModelManager.h"

#include <spdlog/spdlog.h>

namespace systems::edit {
using core::ArgObject;
using std::string;
using std::vector;

const string EditSystem::name = "EditSystem";

EditSystem::EditSystem(ModelManager& model_manager)
    : model_manager_(&model_manager)
{
}

EditSystem::~EditSystem() = default;

std::any EditSystem::call(const string& unique_name, Index model, const vector<ArgObject>& args)
{
    std::optional model_op = model_manager_->getModelOperator(model);
    if (!model_op) {
        spdlog::error("EditSystem::call: Model operator for model ID {} not found.", model);
        return {};
    }
    ModelData& data_op = model_op->data();

    auto it = handlers_.find(unique_name);
    if (it != handlers_.end() && it->second) {
        data_op = it->second->execute(std::move(data_op), args);
        model_op->notifyChanged();
    }
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
}
