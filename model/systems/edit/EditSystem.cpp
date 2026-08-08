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
    auto it = handlers_.find(unique_name);
    if (it != handlers_.end() && it->second) {
        // 过渡 shim（随系统迁移消亡）：操作边界统一 flush 组件变更通知，
        // handler 写路径经 ComponentOperator 写必脏记入待通知集合；无写入则 flush 空转。
        // 异常时先 flush 再重抛，保证部分写入的通知不丢。
        // component_id 仅作对象树选中态提示透传给 handler（可为 -1），目标组件由 handler 按参数解析。
        try {
            std::any result = it->second->execute(*model_manager_, component_id, args);
            model_manager_->flushNotifications();
            return result;
        } catch (...) {
            model_manager_->flushNotifications();
            throw;
        }
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
