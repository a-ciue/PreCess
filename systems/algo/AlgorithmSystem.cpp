/**
 * @file AlgorithmSystem.cpp
 * @author (your name)
 */
#include "AlgorithmSystem.h"
#include "AlgorithmHandler.h"
#include "ModelManager.h"
#include "ModelIOSystem.h"
#include <cassert>
#include <spdlog/spdlog.h>

namespace systems::algo {
const string AlgorithmSystem::name = "AlgorithmSystem";

AlgorithmSystem::AlgorithmSystem(io::ModelIOSystem& io_system, ModelManager& model_manager)
    : io_system_(&io_system)
    , model_manager_(&model_manager)
{
}

std::any AlgorithmSystem::call(const string& unique_name, Index model, const vector<std::any>& args)
{
    std::optional model_op = model_manager_->getModelOperator(model);
    if (!model_op) {
        spdlog::error("AlgorithmSystem::call: Model operator for model ID {} not found.", model);
        return {};
    }

    HandlerContext context {
        *this->io_system_,
        *model_op
    };
    auto it = handlers_.find(unique_name);
    if (it != handlers_.end() && it->second) {
        return it->second->execute(context, args);
    }
    return {};
}

bool AlgorithmSystem::registerHandler(const HandlerMetaData& meta_data, std::shared_ptr<AlgorithmHandler> handler)
{
    if (!handler)
        return false;
    handlers_[meta_data.name] = handler;

    auto info = std::make_unique<AlgorithmInfo>();
    info->name = meta_data.name;
    info->display_name = meta_data.display_name;
    info->arg_types = handler->args_type();
    this->algorithm_infos_[meta_data.name] = std::move(info);

    spdlog::info("AlgorithmSystem::registerHandler: Registered handler for algorithm '{}'", meta_data.name);
    return true;
}

void AlgorithmSystem::unregisterHandler(const HandlerMetaData& meta_data)
{
    if (handlers_.count(meta_data.name) == 0) {
        spdlog::warn("AlgorithmSystem::unregisterHandler: Handler for algorithm '{}' not found", meta_data.name);
    }

    handlers_.erase(meta_data.name);
    this->algorithm_infos_.erase(meta_data.name);

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
}
