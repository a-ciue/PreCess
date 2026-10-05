/**
 * @file FeatureSystem.cpp
 */
#include "FeatureSystem.h"
#include "FeatureHandler.h"
#include "FeatureRegistrar.h"
#include "JobRunner.h"
#include "ModelLayer.h"
#include "ModelScope.h"
#include "ShadowComponent.h"
#include "UndoStack.h"

#include <spdlog/spdlog.h>

#include <optional>
#include <utility>

namespace systems::feature {
const std::string FeatureSystem::name = "FeatureSystem";

FeatureSystem::FeatureEntry::FeatureEntry(FeatureSystem& system, const HandlerMetaData& meta_data)
    : info { .name = meta_data.name,
        .display_name = meta_data.display_name,
        .description = meta_data.description,
        .interactive = meta_data.interactive }
    , params(std::vector<core::ArgType> { })
    , interaction_context(system, interaction_state)
    , event_gateway(*system.event_bus_, *system.model_layer_, system.undo_stack_, meta_data.name,
          [&system](std::function<void()> replay) { system.deferEvent(std::move(replay)); })
    , context(system, meta_data.name, event_gateway, params, interaction_context)
{
}

FeatureSystem::FeatureSystem(ModelLayer& model_layer, core::EventBus& event_bus, UndoStack* undo_stack)
    : model_layer_(&model_layer)
    , event_bus_(&event_bus)
    , undo_stack_(undo_stack)
{
    on_feature_infos_changed_ = []() { };
}

FeatureSystem::~FeatureSystem()
{
    // 原生宿主拆解同样先停在飞任务，避免取消请求后提前回滚真实模型。
    if (job_runner_) {
        if (auto job = job_runner_->currentJob(); job && entries_.contains(job->owner())) {
            // 拆解不再进入此前待切换的目标，先让在飞计算和已有清理实际退出。
            if (pending_feature_)
                pending_feature_ = "";
            job_runner_->stop();
        }
    }
    // 系统析构前先退出当前功能、再停用所有功能，让其清理状态（teardown 清理可能写模型，统一 flush）
    exitCurrentFeature();
    for (auto&& [feature_name, entry] : entries_) {
        if (entry.handler) {
            flushAfterCallback([&] { entry.handler->teardown(entry.context); });
        }
    }
    // 层兜底：系统拆解前关闭残留层（teardown 后、flush 前——恢复 before₀ 自带 flush）
    if (undo_stack_)
        undo_stack_->cancelScope();
    model_layer_->flushNotifications();
}

void FeatureSystem::cancelPendingJobs(const std::string& owner)
{
    if (!owner.empty() && job_runner_)
        if (auto job = job_runner_->currentJob(); job && job->owner() == owner)
            job->cancel();
}

bool FeatureSystem::registerHandler(const HandlerMetaData& meta_data, SystemHandlerPtr handler)
{
    model_layer_->assertOperationIdle();
    if (!handler) {
        return false;
    }
    // 同名功能先注销旧的再替换
    if (entries_.count(meta_data.name)) {
        spdlog::warn("FeatureSystem::registerHandler: feature '{}' already registered, replacing it", meta_data.name);
        unregisterHandler(meta_data);
    }

    // 节点内就地构造全部服务；只保留引用，不跨 setup 持有可能因扩容失效的迭代器。
    FeatureEntry& entry = entries_.try_emplace(meta_data.name, *this, meta_data).first->second;

    // setup 收集声明并订阅事件；失败则撤掉整个条目，不留下半注册状态
    FeatureRegistrar registrar;
    try {
        handler->setup(registrar, entry.context);
    } catch (...) {
        entries_.erase(meta_data.name);
        throw;
    }
    // setup 返回后载入默认值；参数对象地址不变，context 内部引用继续有效。
    entry.params = FeatureParams(registrar.argTypes());
    entry.info.arg_types = registrar.argTypes();
    entry.info.menus = registrar.menuItems();
    entry.info.key_bindings = registrar.keyBindings();
    entry.handler = std::move(handler);

    spdlog::info("FeatureSystem::registerHandler: Registered feature '{}'", meta_data.name);
    on_feature_infos_changed_();
    return true;
}

void FeatureSystem::unregisterHandler(const HandlerMetaData& meta_data)
{
    model_layer_->assertOperationIdle();
    auto it = entries_.find(meta_data.name);
    if (it == entries_.end()) {
        spdlog::warn("FeatureSystem::unregisterHandler: Feature '{}' not found", meta_data.name);
        return;
    }
    // 注销同步完成；活动功能复用切出流程，再 teardown 与移除条目。
    if (current_feature_ == meta_data.name)
        exitCurrentFeature();
    if (it->second.handler) {
        flushAfterCallback([&] { it->second.handler->teardown(it->second.context); });
    }
    // 层兜底：注销必关残留层（非 active 路径此处生效；active 路径切出时已关——幂等早退）
    if (undo_stack_)
        undo_stack_->cancelScope();
    entries_.erase(it);
    spdlog::info("FeatureSystem::unregisterHandler: Unregistered feature '{}'", meta_data.name);
    on_feature_infos_changed_();
}

std::any FeatureSystem::invoke(const std::string& unique_name)
{
    if (model_layer_->writesPending() && !model_layer_->hasWritePrivilege()) {
        spdlog::warn("FeatureSystem::invoke: '{}' rejected while model operation is pending", unique_name);
        return { };
    }
    auto it = entries_.find(unique_name);
    if (it == entries_.end() || !it->second.handler) {
        spdlog::error("FeatureSystem::invoke: Feature '{}' not found", unique_name);
        return { };
    }

    // 操作边界（统一装配，无模式分支）：execute 返回后统一 flush 本次操作的组件变更通知；
    // ModelScope 析构收尾与退出原因无关：异常时先 flush 再传播，部分写入的通知不丢。
    // 正式边界以功能显示名记录，收尾吸收并关闭本功能预览；插件不配对提交。
    // 异常路径同样保留部分效果的可撤销语义。
    FeatureEntry& entry = it->second;
    // 所有者上下文（先于边界构造、后于其析构）：本功能开的边界/层带功能唯一名——
    // 自己的 execute 使用并在收尾吸收预览；其他正式操作开边界即撤销
    ModelScope scope(*model_layer_, undo_stack_, entry.info.display_name,
        ModelScope::Kind::Execute, unique_name);
    const auto previous_job = job_runner_ ? job_runner_->currentJob() : nullptr;
    try {
        return entry.handler->execute(entry.context);
    } catch (...) {
        if (job_runner_)
            if (auto job = job_runner_->currentJob(); job && job != previous_job && job->owner() == unique_name)
                job->cancel();
        throw;
    }
}

void FeatureSystem::flushAfterCallback(const std::function<void()>& fn)
{
    // 生命周期回调（setup/teardown/activate/deactivate）不成记录：仅 flush 边界——
    // ModelScope 析构收尾与退出原因无关（异常路径同样 flush 再传播）
    ModelScope scope(*model_layer_, undo_stack_, { }, ModelScope::Kind::Cleanup);
    fn();
}

bool FeatureSystem::setParameter(const std::string& unique_name, std::size_t index, core::ArgObject value)
{
    auto it = entries_.find(unique_name);
    if (it == entries_.end()) {
        spdlog::error("FeatureSystem::setParameter: Feature '{}' not found", unique_name);
        return false;
    }
    if (index >= it->second.params.count()) {
        spdlog::error("FeatureSystem::setParameter: param index {} out of range for feature '{}'", index, unique_name);
        return false;
    }
    it->second.params.setValue(index, value);
    event_bus_->publish(ParameterChangedEvent { unique_name, index, std::move(value) });
    return true;
}

std::vector<FeatureInfo*> FeatureSystem::getFeatureInfos()
{
    std::vector<FeatureInfo*> infos;
    infos.reserve(entries_.size());
    for (auto&& [feature_name, entry] : entries_) {
        infos.push_back(&entry.info);
    }
    return infos;
}

const FeatureParams* FeatureSystem::params(const std::string& unique_name) const
{
    auto it = entries_.find(unique_name);
    return it == entries_.end() ? nullptr : &it->second.params;
}

interaction::InteractionState* FeatureSystem::activeInteraction()
{
    for (auto&& [feature_name, entry] : entries_) {
        if (entry.info.interactive && entry.interaction_state.active) {
            return &entry.interaction_state;
        }
    }
    return nullptr;
}

bool FeatureSystem::setFeatureActive(const std::string& unique_name)
{
    model_layer_->assertOwnerThread();
    if (!unique_name.empty()) {
        auto target = entries_.find(unique_name);
        if (target == entries_.end() || !target->second.handler) {
            spdlog::warn("FeatureSystem::setFeatureActive: feature '{}' not found", unique_name);
            return false;
        }
    }
    if (model_layer_->writesPending()) {
        if (!pending_feature_ && unique_name == current_feature_)
            return true;
        // 即使当前处于清理授权段，也只登记目标，避免生命周期回调重入销毁条目。
        if (!pending_feature_ && !deferCleanup([this] {
                auto target = std::exchange(pending_feature_, std::nullopt);
                applyFeatureActive(*target);
            }))
            return false;
        pending_feature_ = unique_name;
        cancelPendingJobs(current_feature_);
        return true;
    }
    applyFeatureActive(unique_name);
    return true;
}

void FeatureSystem::exitCurrentFeature()
{
    if (current_feature_.empty())
        return;
    auto cur = entries_.find(current_feature_);
    if (cur != entries_.end() && cur->second.handler) {
        // deactivate 的渲染清理先挂上，再回滚预览、折叠会话并下线交互。
        flushAfterCallback([&] { cur->second.handler->deactivate(cur->second.context); });
        if (undo_stack_) {
            undo_stack_->cancelScope();
            undo_stack_->endSession(current_feature_);
        }
        if (cur->second.info.interactive)
            cur->second.interaction_context.setActive(false);
    }
    current_feature_.clear();
}

void FeatureSystem::applyFeatureActive(const std::string& unique_name)
{
    if (unique_name == current_feature_)
        return;
    exitCurrentFeature();
    if (unique_name.empty())
        return;
    FeatureEntry& entry = entries_.at(unique_name);
    {
        ModelScope scope(*model_layer_, undo_stack_, { }, ModelScope::Kind::Notify);
        entry.handler->activate(entry.context);
    }
    if (entry.info.interactive)
        entry.interaction_context.setActive(true);
    current_feature_ = unique_name;
    if (undo_stack_)
        undo_stack_->beginSession(unique_name, entry.info.display_name);
}

void FeatureSystem::setOnFeatureInfosChanged(std::function<void()> callback)
{
    on_feature_infos_changed_ = std::move(callback);
}

void FeatureSystem::setActiveModelProvider(std::function<std::optional<Index>()> provider)
{
    active_model_provider_ = std::move(provider);
}

void FeatureSystem::setActiveComponentProvider(std::function<std::optional<Index>()> provider)
{
    active_component_provider_ = std::move(provider);
}

void FeatureSystem::setRenderRefreshCallback(std::function<void()> callback)
{
    render_refresh_callback_ = std::move(callback);
}

void FeatureSystem::deactivateOtherInteractions(const interaction::InteractionState& state)
{
    for (auto&& [name, entry] : entries_)
        if (&entry.interaction_state != &state)
            entry.interaction_state.active = false;
}

void FeatureSystem::notifyRenderRefresh()
{
    if (render_refresh_callback_)
        render_refresh_callback_();
}

void FeatureSystem::setJobRunner(systems::job::JobRunner* runner)
{
    model_layer_->assertOperationIdle();
    if (runner)
        runner->assertHost(*model_layer_, undo_stack_);
    job_runner_ = runner;
}

std::shared_ptr<systems::job::Job> FeatureSystem::submitWriteJob(std::string label, const std::string& owner,
    std::function<systems::job::JobWork()> prepare, std::optional<std::uint64_t> preview_scope, bool masked)
{
    if (!job_runner_)
        return nullptr;
    return job_runner_->run(label, [this, prepare = std::move(prepare), label, owner, preview_scope]() mutable {
        auto work = prepare();
        if (!work.task)
            return work;
        work.commit = [this, apply = std::move(work.commit), label, owner, preview_scope](systems::job::ProgressFn report) {
            ModelScope scope(*model_layer_, undo_stack_, label,
                preview_scope ? ModelScope::Kind::Preview : ModelScope::Kind::Execute, owner);
            if (preview_scope && undo_stack_->scopeId() != *preview_scope)
                throw ModelOperationBusy("FeatureSystem: preview result no longer owns its scope");
            apply(std::move(report));
        };
        return work; }, { owner, masked, false, !preview_scope });
}

std::shared_ptr<systems::job::Job> FeatureSystem::submitFreeJob(std::string name,
    systems::job::JobTaskFn task, const std::string& owner)
{
    if (!job_runner_)
        return nullptr;
    return job_runner_->run(std::move(name), [task = std::move(task)]() mutable { return systems::job::JobWork { std::move(task), { } }; }, { owner });
}

std::shared_ptr<systems::job::Job> FeatureSystem::submitCapturedWriteback(std::string label,
    Index component_id, CaptureJobFn capture, WritebackFn write, const std::string& owner, bool masked)
{
    return submitCapturedWriteback(std::move(label), [this, component_id, capture = std::move(capture)](const ModelLayer&) {
        auto target = model_layer_->getComponentOperator(component_id);
        if (!target)
            return systems::job::JobTaskFn { };
        return capture(*target); }, [component_id, write = std::move(write)](ModelLayer& layer, systems::job::ProgressFn report) {
        auto target = layer.getComponentOperator(component_id);
        if (!target)
            throw std::runtime_error("FeatureSystem: occupied writeback target disappeared");
        write(*target, std::move(report)); }, owner, masked);
}

std::shared_ptr<systems::job::Job> FeatureSystem::submitCapturedWriteback(std::string label,
    LayerCaptureJobFn capture, LayerWritebackFn write, const std::string& owner, bool masked)
{
    std::optional<std::uint64_t> preview_scope;
    if (undo_stack_) {
        // 同步 execute 内发布属于正式操作；事件只能回写自己的有效预览。
        if (undo_stack_->inPreviewCallback() || !undo_stack_->inOperation()) {
            const auto id = undo_stack_->scopeId();
            if (!id || undo_stack_->inOperation())
                return nullptr;
            preview_scope = id;
        }
    }
    return submitWriteJob(std::move(label), owner, [this, capture = std::move(capture), write = std::move(write)] {
        // 已占用且无写授权；插件捕获误写在模型底层拒绝。
        return systems::job::JobWork { capture(*model_layer_), [this, write](systems::job::ProgressFn report) { write(*model_layer_, std::move(report)); } }; }, preview_scope, masked);
}

std::shared_ptr<systems::job::Job> FeatureSystem::submitModelJob(std::string label, Index component_id,
    ModelJobTaskFn task, const std::string& owner)
{
    if (undo_stack_ && (!undo_stack_->inOperation() || undo_stack_->inPreviewCallback() || undo_stack_->scopeActive()))
        return nullptr;
    return submitWriteJob(label, owner, [this, component_id, task = std::move(task)] {
        auto target = model_layer_->getComponentOperator(component_id);
        if (!target || target->component().geometry || target->component().mapping)
            return systems::job::JobWork { };
        auto shadow = std::make_shared<systems::job::ShadowComponent>(*target);
        return systems::job::JobWork { [shadow, task](systems::job::ProgressFn report) {
            auto target = shadow->target();
            task(target, std::move(report));
            shadow->finishCompute();
        }, [this, shadow](systems::job::ProgressFn) { shadow->apply(*model_layer_); } }; }, std::nullopt, true);
}

bool FeatureSystem::deferCleanup(std::function<void()> cleanup)
{
    if (!job_runner_)
        return false;
    return job_runner_->deferUntilFinished([lifetime = std::weak_ptr<int>(lifetime_), cleanup = std::move(cleanup)] {
        if (!lifetime.expired())
            cleanup();
    });
}

void FeatureSystem::deferEvent(std::function<void()> replay)
{
    model_layer_->assertOwnerThread();
    deferred_events_.push_back(std::move(replay));
}

void FeatureSystem::flushDeferredEvents()
{
    model_layer_->assertOwnerThread();
    if (model_layer_->writesPending())
        return; // 迟到终态通知不在后继任务占用期取走通知。
    std::vector<std::function<void()>> pending;
    pending.swap(deferred_events_);
    for (auto& replay : pending) {
        try {
            replay();
        } catch (const std::exception& e) {
            // 发布者已错过，异常不外传（重放位于 GUI 事件处理栈内）：记日志保其余重放继续
            spdlog::error("FeatureSystem::flushDeferredEvents: deferred callback failed: {}", e.what());
        } catch (...) {
            spdlog::error("FeatureSystem::flushDeferredEvents: deferred callback failed with unknown exception");
        }
    }
}

bool FeatureSystem::dispatchKeyEvent(const KeyEvent& event)
{
    // 冻结期按键整体不派发（原始流与绑定路由双路径跳过）：遮罩已挡用户输入，
    // 此处为程序化派发（如自动化测试/脚本注入）的兜底——模型与影子须保持同源
    if (model_layer_->writesPending())
        return false;
    // 原始事件流先广播，观察者总能收到；再做按键绑定路由并返回消费结果。
    // 路由可能更新本功能预览；通知作用域也在异常退出时收尾，不建立正式记录。
    event_bus_->publish(event);
    ModelScope scope(*model_layer_, undo_stack_, { }, ModelScope::Kind::Notify);
    return routeKeyEvent(event);
}

bool FeatureSystem::routeKeyEvent(const KeyEvent& event)
{
    if (!event.pressed) {
        return false; // 按键绑定仅在按下时触发；释放流由功能自行订阅 KeyEvent
    }
    bool consumed = false;
    for (auto&& [feature_name, entry] : entries_) {
        for (const KeyBinding& binding : entry.info.key_bindings) {
            if (binding.key == event.key && binding.modifiers == event.modifiers) {
                // 显式 execute 声明走正式入口；普通按键回调只获得预览访问。
                // 通知由外层 dispatchKeyEvent 统一 flush。
                UndoStack::OwnerScope owner_scope(undo_stack_, feature_name);
                if (binding.execute) {
                    invoke(feature_name);
                    consumed = true;
                } else {
                    UndoStack::PreviewAccess preview_access(undo_stack_);
                    try {
                        consumed = entry.handler->onKeyEvent(event) || consumed;
                    } catch (const ModelOperationBusy& e) {
                        spdlog::warn("FeatureSystem: key callback rejected: {}", e.what());
                    }
                }
            }
        }
    }
    return consumed;
}
}
