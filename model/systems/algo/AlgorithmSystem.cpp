/**
 * @file AlgorithmSystem.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "AlgorithmSystem.h"
#include "AlgorithmHandler.h"
#include "ArgObject.h"
#include "ModelIOSystem.h"
#include "ModelLayer.h"
#include <cassert>
#include <spdlog/spdlog.h>

namespace systems::algo {
using core::ArgObject;
using std::string;
using std::vector;

const string AlgorithmSystem::name = "AlgorithmSystem";

AlgorithmSystem::AlgorithmSystem(io::ModelIOSystem& io_system, ModelLayer& model_manager)
    : io_system_(&io_system)
    , model_manager_(&model_manager)
{
    on_algorithm_infos_changed_ = []() { };
}

AlgorithmSystem::~AlgorithmSystem() = default;

std::any AlgorithmSystem::call(const string& unique_name, Index component_id, const vector<ArgObject>& args)
{
    auto comp_op = model_manager_->getComponentOperator(component_id);
    if (!comp_op) {
        spdlog::error("AlgorithmSystem::call: ComponentData operator for component ID {} not found.", component_id);
        return {};
    }

    HandlerContext context {
        *this->io_system_,
        *comp_op
    };
    auto it = handlers_.find(unique_name);
    if (it != handlers_.end() && it->second) {
        return it->second->execute(context, args);
    }
    return {};
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
