/**
 * @file AlgorithmSystem.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "AlgorithmSystem.h"
#include "AlgorithmHandler.h"
#include "ArgObject.h"
#include "JobRunner.h"
#include "ModelData.h"
#include "ModelIOSystem.h"
#include "ModelLayer.h"
#include "ModelPayload.h"
#include "ModelScope.h"
#include "ShadowComponent.h"
#include "UndoStack.h"
#include <algorithm>
#include <cassert>
#include <map>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <utility>

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

std::any AlgorithmSystem::call(const string& unique_name, Index component_id, const vector<ArgObject>& args,
    ProgressFn progress)
{
    std::unique_ptr<ModelLayer::WriteOperation> operation;
    if (!model_manager_->hasWritePrivilege()) {
        operation = model_manager_->beginWriteOperation(true);
        if (!operation) {
            spdlog::warn("AlgorithmSystem::call: '{}' rejected while another model operation is pending", unique_name);
            return { };
        }
    }
    ModelLayer::WritePrivilege write_privilege(*model_manager_, operation.get());

    auto it = entries_.find(unique_name);
    if (it == entries_.end()) {
        spdlog::warn("AlgorithmSystem::call: Handler '{}' not found.", unique_name);
        return { };
    }

    if (undo_stack_)
        undo_stack_->prepareOperation();
    const auto target_component_id = it->second.handler->resolveComponentId(
        *model_manager_, component_id, args);
    if (!target_component_id) {
        spdlog::error(
            "AlgorithmSystem::call: Cannot resolve target component for algorithm '{}'.",
            unique_name);
        return { };
    }

    auto comp_op = model_manager_->getComponentOperator(*target_component_id);
    if (!comp_op) {
        spdlog::error("AlgorithmSystem::call: ComponentData operator for component ID {} not found.",
            *target_component_id);
        return { };
    }

    return executeAlgorithm(it->second, *comp_op, args, std::move(progress));
}

std::any AlgorithmSystem::executeAlgorithm(const AlgorithmEntry& entry, ComponentOperator& target,
    const vector<ArgObject>& args, ProgressFn progress, const std::string& owner)
{
    HandlerContext context { *io_system_, target };
    // 上层注入了进度回调则覆写 no-op 默认值；空则保持 no-op，handler 上报不判空也不崩
    if (progress)
        context.report_progress = std::move(progress);
    // 同步与 inline 均在既有授权段执行；异常也保留部分效果的可撤销语义。
    const std::string& display_name = entry.info.display_name;
    ModelScope scope(*model_manager_, undo_stack_, display_name.empty() ? entry.info.name : display_name,
        ModelScope::Kind::Command, owner);
    return entry.handler->execute(context, args);
}

// —— 影子执行（方案二 S3）：计算与提交分离 ——

struct PreparedAlgorithm {
    std::string label;
    AlgorithmHandler* handler;
    Index real_component_id;
    Index real_model_id;
    std::vector<core::ArgObject> args;
    systems::job::ShadowComponent shadow;
    std::vector<ModelPayload> imports;
    PreparedAlgorithm(const ComponentOperator& target, AlgorithmHandler& algorithm,
        std::string label, std::vector<core::ArgObject> args)
        : label(std::move(label))
        , handler(&algorithm)
        , real_component_id(target.componentId())
        , real_model_id(target.modelId())
        , args(std::move(args))
        , shadow(target)
    {
    }
};

/**
 * @brief 影子 IO 适配：read 解析后进收养清单（不落真实层）；
 *        write* 经 id 映射转发真实系统（算法执行期模型冻结，真实层仅被只读访问）
 */
class ShadowIOSystem final : public systems::io::ModelIOSystemBase {
public:
    ShadowIOSystem(PreparedAlgorithm& prep, systems::io::ModelIOSystem& real)
        : prep_(prep)
        , real_(real)
    {
    }

    bool read(const std::filesystem::path& path, const std::string& file_type,
        const std::vector<std::any>& args) override
    {
        auto payload = real_.parseModel(path, file_type, args);
        if (!payload)
            return false;
        prep_.imports.push_back(std::move(*payload));
        return true;
    }

    void write(Index model, const std::filesystem::path& path, const std::string& file_type,
        const std::vector<std::any>& args) override
    {
        if (model == prep_.real_model_id) {
            real_.write(model, path, file_type, args);
            return;
        }
        spdlog::error("ShadowIOSystem::write: model {} not visible to shadow execution", model);
        throw std::runtime_error("write target outside shadow scope");
    }

    void writeComponents(const std::vector<Index>& component_ids, const std::filesystem::path& path,
        const std::string& file_type, const std::vector<std::any>& args) override
    {
        std::vector<Index> mapped;
        mapped.reserve(component_ids.size());
        for (Index component_id : component_ids) {
            if (component_id == prep_.shadow.componentId()) {
                mapped.push_back(prep_.real_component_id);
                continue;
            }
            spdlog::error("ShadowIOSystem::writeComponents: component {} not visible to shadow execution",
                component_id);
            throw std::runtime_error("write target outside shadow scope");
        }
        real_.writeComponents(mapped, path, file_type, args);
    }

private:
    PreparedAlgorithm& prep_;
    systems::io::ModelIOSystem& real_;
};

void AlgorithmSystem::setJobRunner(systems::job::JobRunner* runner)
{
    model_manager_->assertOperationIdle();
    if (runner)
        runner->assertHost(*model_manager_, undo_stack_);
    job_runner_ = runner;
}
std::shared_ptr<systems::job::Job> AlgorithmSystem::callAsync(std::string unique_name,
    Index component_id, std::vector<core::ArgObject> args)
{
    if (!job_runner_)
        return nullptr;
    const auto owner = undo_stack_ ? undo_stack_->currentOwner() : std::string { };
    return job_runner_->run(unique_name, [this, unique_name, component_id, args = std::move(args), owner]() mutable {
        return prepareAlgorithm(unique_name, component_id, std::move(args), owner);
    },
        { { }, true, true });
}

systems::job::JobWork AlgorithmSystem::prepareAlgorithm(const std::string& unique_name,
    Index component_id, std::vector<core::ArgObject> args, const std::string& owner)
{
    auto it = entries_.find(unique_name);
    if (it == entries_.end())
        throw std::runtime_error("handler not found");
    const auto target_id = it->second.handler->resolveComponentId(*model_manager_, component_id, args);
    if (!target_id)
        throw std::runtime_error("cannot resolve target component");
    auto target = model_manager_->getComponentOperator(*target_id);
    if (!target)
        throw std::runtime_error("component operator not found");
    // 几何及映射身份不能跨层移植，保持原生同步路径。
    if (target->component().geometry || target->component().mapping)
        return systems::job::JobWork { [this, entry = &it->second, id = *target_id, args = std::move(args), owner](ProgressFn report) {
                                          // 占用期注册节点保持稳定；按已解析身份重新取写面，不重新解析选择器。
                                          auto target = model_manager_->getComponentOperator(id);
                                          if (!target)
                                              throw std::runtime_error("occupied inline target disappeared");
                                          executeAlgorithm(*entry, *target, args, std::move(report), owner);
                                      },
            { }, true };
    const auto label = !it->second.info.display_name.empty()
        ? it->second.info.display_name
        : unique_name;
    auto prep = std::make_shared<PreparedAlgorithm>(*target, *it->second.handler, label, std::move(args));
    return systems::job::JobWork { [this, prep](ProgressFn report) {
                                      computeShadow(*prep, std::move(report));
                                  },
        [this, prep, owner](ProgressFn) {
            ModelScope scope(*model_manager_, undo_stack_, prep->label, ModelScope::Kind::Command, owner);
            applyShadow(*prep);
        } };
}

void AlgorithmSystem::computeShadow(PreparedAlgorithm& prep, ProgressFn progress)
{
    auto target = prep.shadow.target();
    ShadowIOSystem io(prep, *io_system_);
    HandlerContext context { io, target };
    if (progress)
        context.report_progress = std::move(progress);
    prep.handler->execute(context, prep.args);
    prep.shadow.finishCompute();
}

void AlgorithmSystem::applyShadow(PreparedAlgorithm& prep)
{
    prep.shadow.apply(*model_manager_);
    // IO 导入是算法专属，影子组件不负责收养模型。
    for (auto& payload : prep.imports)
        model_manager_->addModel(u8Narrow(payload.model_name), std::move(payload.components));
}

bool AlgorithmSystem::registerHandler(const HandlerMetaData& meta_data, SystemHandlerPtr handler)
{
    model_manager_->assertOperationIdle();
    if (!handler)
        return false;

    // 参数准备成功后再替换注册，保留旧接口既有的失败原子性。
    AlgorithmInfo info { .name = meta_data.name,
        .display_name = meta_data.display_name,
        .arg_types = handler->args_type() };
    entries_.insert_or_assign(meta_data.name, AlgorithmEntry { std::move(handler), std::move(info) });
    spdlog::info("AlgorithmSystem::registerHandler: Registered handler for algorithm '{}'", meta_data.name);
    on_algorithm_infos_changed_();
    return true;
}

void AlgorithmSystem::unregisterHandler(const HandlerMetaData& meta_data)
{
    // 计算借用 handler；真实终态前禁止将其从插件销毁。
    model_manager_->assertOperationIdle();
    if (entries_.erase(meta_data.name) == 0) {
        spdlog::warn("AlgorithmSystem::unregisterHandler: Handler for algorithm '{}' not found", meta_data.name);
    }

    on_algorithm_infos_changed_();

    spdlog::info("AlgorithmSystem::unregisterHandler: Unregistered handler for algorithm '{}'", meta_data.name);
}

vector<AlgorithmInfo*> AlgorithmSystem::getAlgorithmInfos()
{
    vector<AlgorithmInfo*> infos;
    infos.reserve(entries_.size());
    for (auto&& [algo_name, entry] : entries_) {
        infos.push_back(&entry.info);
    }
    return infos;
}

std::optional<std::vector<core::ArgType>> AlgorithmSystem::getArgTypes(const std::string& unique_name)
{
    auto it = entries_.find(unique_name);
    if (it != entries_.end()) {
        return it->second.handler->args_type();
    }
    return { };
}

void AlgorithmSystem::setOnAlgorithmInfosChanged(std::function<void()> callback)
{
    on_algorithm_infos_changed_ = std::move(callback);
}
}
