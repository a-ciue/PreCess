/**
 * @file FeatureContext.cpp
 * @brief 固定功能服务：查询宿主，任务与预览按构造身份交给框架。
 */
#include "FeatureContext.h"
#include "FeatureSystem.h"
#include "ModelLayer.h"
#include "UndoStack.h"

namespace systems::feature {
UndoContext::UndoContext(FeatureSystem& system, std::string owner)
    : system_(system)
    , owner_(std::move(owner))
{
}

bool UndoContext::beginScope(std::string label)
{
    auto* stack = system_.undo_stack_;
    UndoStack::OwnerScope owner_scope(stack, owner_);
    return stack && stack->beginScope(std::move(label));
}

void UndoContext::cancelScope()
{
    auto* stack = system_.undo_stack_;
    UndoStack::OwnerScope owner_scope(stack, owner_);
    if (!stack || stack->scopeId() == 0)
        return;
    if (system_.model_layer_->writesPending() && !system_.model_layer_->hasWritePrivilege()) {
        system_.cancelPendingJobs(owner_);
        if (system_.deferCleanup([stack, owner = owner_, scope_id = stack->scopeId()] {
                UndoStack::OwnerScope owner_scope(stack, owner);
                if (stack->scopeId() == scope_id)
                    stack->cancelScope();
            }))
            return;
    }
    stack->cancelScope();
}

void UndoContext::revertScope()
{
    auto* stack = system_.undo_stack_;
    UndoStack::OwnerScope owner_scope(stack, owner_);
    if (stack && stack->scopeId() != 0)
        stack->revertScope();
}

bool UndoContext::scopeActive() const
{
    auto* stack = system_.undo_stack_;
    UndoStack::OwnerScope owner_scope(stack, owner_);
    return stack && stack->scopeId() != 0;
}

FeatureContext::FeatureContext(FeatureSystem& system, std::string owner, FeatureEventGateway& events,
    FeatureParams& params, InteractionContext& interaction)
    : model(*system.model_layer_)
    , events(events)
    , params(params)
    , interaction(interaction)
    , undo(system, owner)
    , system_(system)
    , owner_(std::move(owner))
{
}

std::optional<Index> FeatureContext::activeModel() const
{
    return system_.active_model_provider_ ? system_.active_model_provider_() : std::nullopt;
}

std::optional<Index> FeatureContext::activeComponent() const
{
    return system_.active_component_provider_ ? system_.active_component_provider_() : std::nullopt;
}

std::optional<ComponentOperator> FeatureContext::componentOperator(Index component_id) const
{
    return model.getComponentOperator(component_id);
}

void FeatureContext::publishResult(std::string text)
{
    model.assertOwnerThread();
    events.bus().publish(FeatureResultEvent { owner_, std::move(text) });
}

std::shared_ptr<systems::job::Job> FeatureContext::runJob(std::string label, systems::job::JobTaskFn task)
{
    UndoStack::OwnerScope owner_scope(system_.undo_stack_, owner_);
    return system_.submitFreeJob(std::move(label), std::move(task), owner_);
}

std::shared_ptr<systems::job::Job> FeatureContext::runModelJob(std::string label, Index component_id, ModelJobTaskFn task)
{
    UndoStack::OwnerScope owner_scope(system_.undo_stack_, owner_);
    return system_.submitModelJob(std::move(label), component_id, std::move(task), owner_);
}

std::shared_ptr<systems::job::Job> FeatureContext::runCapturedWriteback(std::string label, Index component_id,
    CaptureJobFn capture, WritebackFn write, bool masked)
{
    UndoStack::OwnerScope owner_scope(system_.undo_stack_, owner_);
    return system_.submitCapturedWriteback(std::move(label), component_id, std::move(capture), std::move(write), owner_, masked);
}

std::shared_ptr<systems::job::Job> FeatureContext::runCapturedWriteback(std::string label,
    LayerCaptureJobFn capture, LayerWritebackFn write, bool masked)
{
    UndoStack::OwnerScope owner_scope(system_.undo_stack_, owner_);
    return system_.submitCapturedWriteback(std::move(label), std::move(capture), std::move(write), owner_, masked);
}
}
