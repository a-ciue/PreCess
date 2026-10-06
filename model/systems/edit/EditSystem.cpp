/**
 * @file EditSystem.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "EditSystem.h"
#include "ArgObject.h"
#include "EditHandler.h"
#include "ModelLayer.h"
#include "ModelScope.h"
#include "UndoStack.h"

#include <spdlog/spdlog.h>
#include <utility>

namespace systems::edit {
using core::ArgObject;
using std::string;
using std::vector;

const string EditSystem::name = "EditSystem";

EditSystem::EditSystem(ModelLayer& model_manager, UndoStack* undo_stack)
    : model_manager_(&model_manager)
    , undo_stack_(undo_stack)
{
    on_edit_info_changed_ = []() { };
}

EditSystem::~EditSystem() = default;

std::any EditSystem::call(const string& unique_name, Index component_id, const vector<ArgObject>& args)
{
    if (model_manager_->writesPending() && !model_manager_->hasWritePrivilege()) {
        spdlog::warn("EditSystem::call: '{}' rejected while a model operation is pending", unique_name);
        return { };
    }
    auto it = entries_.find(unique_name);
    if (it != entries_.end()) {
        // 过渡 shim（随系统迁移消亡）：操作边界统一 flush 组件变更通知 + undo 自动记录，
        // handler 写路径经 ComponentOperator 写必脏记入待通知集合；无写入则 flush 空转、空操作丢弃。
        // ModelScope 析构收尾与退出原因无关：异常时先提交（部分写入可撤销）+ flush 再传播。
        // component_id 仅作对象树选中态提示透传给 handler（可为 -1），目标组件由 handler 按参数解析。
        const std::string& display_name = it->second.info.display_name;
        ModelScope scope(*model_manager_, undo_stack_, display_name.empty() ? unique_name : display_name);
        return it->second.handler->execute(*model_manager_, component_id, args);
    }

    spdlog::warn("EditSystem::call: Handler '{}' not found.", unique_name);
    return { };
}

bool EditSystem::registerHandler(const HandlerMetaData& meta_data, SystemHandlerPtr handler)
{
    model_manager_->assertOperationIdle();
    if (!handler)
        return false;

    // 元数据准备成功后整体替换，异常不改变旧注册。
    EditInfo info { .name = meta_data.name,
        .display_name = meta_data.display_name,
        .arg_types = handler->args_type() };
    entries_.insert_or_assign(meta_data.name, EditEntry { std::move(handler), std::move(info) });
    on_edit_info_changed_();
    spdlog::info("EditSystem::registerHandler: Registered handler for edit '{}'", meta_data.name);
    return true;
}

void EditSystem::unregisterHandler(const HandlerMetaData& meta_data)
{
    model_manager_->assertOperationIdle();
    if (entries_.erase(meta_data.name) == 0) {
        spdlog::warn("EditSystem::unregisterHandler: Handler for edit '{}' not found", meta_data.name);
    }

    on_edit_info_changed_();

    spdlog::info("EditSystem::unregisterHandler: Unregistered handler for edit '{}'", meta_data.name);
}

vector<EditInfo*> EditSystem::getEditInfos()
{
    vector<EditInfo*> infos;
    infos.reserve(entries_.size());
    for (auto&& [edit_name, entry] : entries_) {
        infos.push_back(&entry.info);
    }
    return infos;
}

std::optional<std::vector<core::ArgType>> EditSystem::getArgTypes(const std::string& unique_name)
{
    auto it = entries_.find(unique_name);
    if (it != entries_.end()) {
        return it->second.handler->args_type();
    }
    return { };
}

void EditSystem::setOnEditInfoChangedCallback(std::function<void()> callback)
{
    on_edit_info_changed_ = std::move(callback);
}
}
