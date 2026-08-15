/**
 * @file AlgorithmSystem.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "AlgorithmSystem.h"
#include "AlgorithmHandler.h"
#include "ArgObject.h"
#include "ModelIOSystem.h"
#include "ModelLayer.h"
#include "UndoStack.h"
#include <cassert>
#include <spdlog/spdlog.h>

namespace systems::algo {
using core::ArgObject;
using std::string;
using std::vector;

const string AlgorithmSystem::name = "AlgorithmSystem";

AlgorithmSystem::AlgorithmSystem(io::ModelIOSystem& io_system, ModelLayer& model_manager, UndoStack* undo_stack)
    : io_system_(&io_system)
    , model_manager_(&model_manager)
    , undo_stack_(undo_stack)
{
    on_algorithm_infos_changed_ = []() { };
}

AlgorithmSystem::~AlgorithmSystem() = default;

std::any AlgorithmSystem::call(const string& unique_name, Index component_id, const vector<ArgObject>& args)
{
    auto it = handlers_.find(unique_name);
    if (it == handlers_.end() || !it->second) {
        spdlog::warn("AlgorithmSystem::call: Handler '{}' not found.", unique_name);
        return {};
    }

    const auto target_component_id = it->second->resolveComponentId(
        *model_manager_, component_id, args);
    if (!target_component_id) {
        spdlog::error(
            "AlgorithmSystem::call: Cannot resolve target component for algorithm '{}'.",
            unique_name);
        return {};
    }

    auto comp_op = model_manager_->getComponentOperator(*target_component_id);
    if (!comp_op) {
        spdlog::error("AlgorithmSystem::call: ComponentData operator for component ID {} not found.",
            *target_component_id);
        return {};
    }

    HandlerContext context {
        *this->io_system_,
        *comp_op
    };
    // 过渡 shim（随系统迁移消亡）：操作边界统一 flush 组件变更通知 + undo 自动记录，
    // handler 写路径经 ComponentOperator 写必脏记入待通知集合；无写入则 flush 空转、空操作丢弃。
    // 异常时先提交（部分写入可撤销）+ flush 再重抛，保证部分写入的通知不丢。
    if (undo_stack_) {
        const std::string& display_name = algorithm_infos_[unique_name]->display_name;
        undo_stack_->beginOperation(display_name.empty() ? unique_name : display_name);
    }
    try {
        std::any result = it->second->execute(context, args);
        if (undo_stack_)
            undo_stack_->commitOperation();
        model_manager_->flushNotifications();
        return result;
    } catch (...) {
        if (undo_stack_)
            undo_stack_->commitOperation();
        model_manager_->flushNotifications();
        throw;
    }
}

bool AlgorithmSystem::registerHandler(const HandlerMetaData& meta_data, SystemHandlerPtr handler)
{
    if (!handler)
        return false;

    auto info = std::make_unique<AlgorithmInfo>();
    info->name = meta_data.name;
    info->display_name = meta_data.display_name;
    info->arg_types = handler->args_type();
    this->algorithm_infos_[meta_data.name] = std::move(info);

    handlers_[meta_data.name] = std::move(handler);
    spdlog::info("AlgorithmSystem::registerHandler: Registered handler for algorithm '{}'", meta_data.name);
    on_algorithm_infos_changed_();
    return true;
}

void AlgorithmSystem::unregisterHandler(const HandlerMetaData& meta_data)
{
    if (handlers_.count(meta_data.name) == 0) {
        spdlog::warn("AlgorithmSystem::unregisterHandler: Handler for algorithm '{}' not found", meta_data.name);
    }

    handlers_.erase(meta_data.name);
    this->algorithm_infos_.erase(meta_data.name);
    on_algorithm_infos_changed_();

    spdlog::info("AlgorithmSystem::unregisterHandler: Unregistered handler for algorithm '{}'", meta_data.name);
}

vector<AlgorithmInfo*> AlgorithmSystem::getAlgorithmInfos()
{
    vector<AlgorithmInfo*> infos;
    infos.reserve(algorithm_infos_.size());
    for (auto&& [algo_name, algo_info] : algorithm_infos_) {
        infos.push_back(algo_info.get());
    }
    return infos;
}

std::optional<std::vector<core::ArgType>> AlgorithmSystem::getArgTypes(const std::string& unique_name)
{
    auto it = handlers_.find(unique_name);
    if (it != handlers_.end() && it->second) {
        return it->second->args_type();
    }
    return {};
}

void AlgorithmSystem::setOnAlgorithmInfosChanged(std::function<void()> callback)
{
    on_algorithm_infos_changed_ = std::move(callback);
}
}
