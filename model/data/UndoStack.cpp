/**
 * @file UndoStack.cpp
 * @brief 撤销/重做栈实现（语义见 UndoStack.h 文件头注释）
 */
#include "UndoStack.h"
#include "ComponentData.h"
#include "ComponentOperator.h"
#include "ModelData.h"
#include "ModelLayer.h"
#include "ModelSnapshot.h"

#include <stdexcept>
#include <utility>

namespace {
//! @brief applying_ 置位守卫：恢复过程抛异常时保证复位，避免记录被永久抑制
struct ApplyingGuard {
    bool& flag;
    explicit ApplyingGuard(bool& f)
        : flag(f)
    {
        flag = true;
    }
    ~ApplyingGuard() { flag = false; }
};
}

UndoStack::UndoStack(ModelLayer& model)
    : model_(&model)
{
}

// —— 操作边界（自动模式）——

void UndoStack::beginOperation(std::string label)
{
    // 未确认切换算法的兜底：staged 打开时先隐式回滚旧预览
    if (staged_)
        cancelStaged();

    if (++op_depth_ == 1) {
        // 最外层边界生效：接管操作标签并清空暂存
        op_label_ = std::move(label);
        op_components_.clear();
        op_structural_.clear();
    }
}

void UndoStack::commitOperation()
{
    if (op_depth_ <= 0)
        return;
    if (--op_depth_ > 0)
        return;

    // 最外层边界收尾：空操作丢弃，否则补 after-image、入栈、清空 redo
    if (op_components_.empty() && op_structural_.empty()) {
        op_label_.clear();
        return;
    }
    for (auto& entry : op_components_) {
        if (ComponentData* c = model_->findComponent(entry.component_id))
            entry.after = c->clone();
        // 组件在边界内被删除：after 留空，恢复由结构条目（ComponentRemoved）负责
    }
    pushRecord(UndoRecord { std::move(op_label_), std::move(op_components_), std::move(op_structural_) });
    op_label_.clear();
    op_components_.clear();
    op_structural_.clear();
}

// —— staged 会话（Manual 模式）——

bool UndoStack::beginStaged(std::string label, Index component_id)
{
    if (staged_)
        throw std::runtime_error("UndoStack::beginStaged: staged session already active");
    ComponentData* c = model_->findComponent(component_id);
    if (!c)
        return false;

    staged_ = StagedSession { std::move(label), component_id, c->clone() };
    if (on_changed_)
        on_changed_();
    return true;
}

void UndoStack::commitStaged()
{
    if (!staged_)
        return;
    StagedSession session = std::move(*staged_);
    staged_.reset();

    // 确认：before₀ + 当前状态合成一条记录；之后 undo 一次整体回滚
    ComponentEntry entry;
    entry.component_id = session.component_id;
    entry.before = std::move(session.before);
    if (ComponentData* c = model_->findComponent(session.component_id))
        entry.after = c->clone();

    UndoRecord record;
    record.label = std::move(session.label);
    record.components.push_back(std::move(entry));
    pushRecord(std::move(record));
}

void UndoStack::cancelStaged()
{
    if (!staged_)
        return;
    StagedSession session = std::move(*staged_);
    staged_.reset();

    // 恢复 before₀ 并关闭会话，不成记录；自身即边界（恢复后 flush）
    {
        ApplyingGuard guard(applying_);
        restoreComponentSnapshot(session.component_id, *session.before);
    }
    model_->flushNotifications();
    if (on_changed_)
        on_changed_();
}

void UndoStack::revertStaged()
{
    if (!staged_)
        return;
    // 恢复 before₀，会话保持打开（纯预览重试）
    {
        ApplyingGuard guard(applying_);
        restoreComponentSnapshot(staged_->component_id, *staged_->before);
    }
    model_->flushNotifications();
    if (on_changed_)
        on_changed_();
}

bool UndoStack::stagedActive() const
{
    return staged_.has_value();
}

// —— 撤销/重做 ——

bool UndoStack::canUndo() const
{
    // staged 打开：undo = cancelStaged（回滚预览），有意义
    return staged_.has_value() || !undo_.empty();
}

bool UndoStack::canRedo() const
{
    // staged 打开时 redo 空转（无环节概念）
    if (staged_)
        return false;
    return !redo_.empty();
}

std::optional<std::string> UndoStack::undoLabel() const
{
    if (staged_)
        return staged_->label;
    if (undo_.empty())
        return std::nullopt;
    return undo_.back().label;
}

std::optional<std::string> UndoStack::redoLabel() const
{
    if (staged_)
        return std::nullopt;
    if (redo_.empty())
        return std::nullopt;
    return redo_.back().label;
}

void UndoStack::undo()
{
    // staged 打开：undo = cancelStaged（恢复 before₀ 并关闭会话，消费本次），全局栈记录不动
    if (staged_) {
        cancelStaged();
        return;
    }

    if (undo_.empty())
        return;
    UndoRecord record = std::move(undo_.back());
    undo_.pop_back();

    {
        ApplyingGuard guard(applying_);
        // 先逆序回滚结构变更（先恢复被删组件/模型），再回滚组件数据（before-image）。
        // 顺序约束：组件数据恢复要求组件已存在（由结构回滚保证）；结构条目 undo 时
        // 重新捕获的快照存回记录供 redo 使用。
        for (auto it = record.structural.rbegin(); it != record.structural.rend(); ++it) {
            switch (it->kind) {
            case StructuralEntry::Kind::ModelAdded:
                it->model_snapshot = model_->takeModelSnapshot(it->model_id);
                model_->removeModel(it->model_id);
                break;
            case StructuralEntry::Kind::ModelRemoved:
                model_->restoreModel(*it->model_snapshot);
                break;
            case StructuralEntry::Kind::ComponentAdded:
                if (ComponentData* c = model_->findComponent(it->component_id)) {
                    it->component_snapshot = c->clone();
                    model_->removeComponent(it->component_id);
                }
                break;
            case StructuralEntry::Kind::ComponentRemoved:
                model_->restoreComponent(it->model_id, it->component_snapshot->clone());
                break;
            }
        }
        for (auto& entry : record.components) {
            if (entry.before)
                restoreComponentSnapshot(entry.component_id, *entry.before);
        }
    }
    model_->flushNotifications();

    redo_.push_back(std::move(record));
    if (on_changed_)
        on_changed_();
}

void UndoStack::redo()
{
    // staged 打开：redo 空转（无环节概念）
    if (staged_)
        return;

    if (redo_.empty())
        return;
    UndoRecord record = std::move(redo_.back());
    redo_.pop_back();

    {
        ApplyingGuard guard(applying_);
        // 与 undo 对称：先按 after-image 重做组件数据，再正序重做结构变更；
        // 结构条目 redo 时重新捕获的快照存回记录供再次 undo 使用。
        for (auto& entry : record.components) {
            if (entry.after)
                restoreComponentSnapshot(entry.component_id, *entry.after);
        }
        for (auto& entry : record.structural) {
            switch (entry.kind) {
            case StructuralEntry::Kind::ModelAdded:
                model_->restoreModel(*entry.model_snapshot);
                break;
            case StructuralEntry::Kind::ModelRemoved:
                entry.model_snapshot = model_->takeModelSnapshot(entry.model_id);
                model_->removeModel(entry.model_id);
                break;
            case StructuralEntry::Kind::ComponentAdded:
                model_->restoreComponent(entry.model_id, entry.component_snapshot->clone());
                break;
            case StructuralEntry::Kind::ComponentRemoved:
                if (ComponentData* c = model_->findComponent(entry.component_id)) {
                    entry.component_snapshot = c->clone();
                    model_->removeComponent(entry.component_id);
                }
                break;
            }
        }
    }
    model_->flushNotifications();

    undo_.push_back(std::move(record));
    if (on_changed_)
        on_changed_();
}

void UndoStack::clear()
{
    undo_.clear();
    redo_.clear();
    if (on_changed_)
        on_changed_();
}

// —— UndoRecorder 钩子 ——

void UndoStack::onComponentDirty(Index component_id, const ComponentData& data)
{
    // first-dirty 捕获 before-image：仅在最外层操作边界内、每组件一次；
    // applying_（恢复中）与边界外（staged 会话由链自持）不记录
    if (applying_ || op_depth_ <= 0)
        return;
    for (const auto& entry : op_components_) {
        if (entry.component_id == component_id)
            return;
    }
    op_components_.push_back(ComponentEntry { component_id, data.clone(), nullptr });
}

void UndoStack::onModelAdded(Index model_id)
{
    if (applying_)
        return;
    StructuralEntry entry;
    entry.kind = StructuralEntry::Kind::ModelAdded;
    entry.model_id = model_id;
    recordStructural(std::move(entry));
}

void UndoStack::onModelRemoving(const ModelData& model)
{
    if (applying_)
        return;
    const Index model_id = model_->findModelId(model);
    if (model_id < 0)
        return;
    StructuralEntry entry;
    entry.kind = StructuralEntry::Kind::ModelRemoved;
    entry.model_id = model_id;
    entry.model_snapshot = model_->takeModelSnapshot(model_id);
    recordStructural(std::move(entry));
}

void UndoStack::onComponentAdded(Index model_id, Index component_id)
{
    if (applying_)
        return;
    StructuralEntry entry;
    entry.kind = StructuralEntry::Kind::ComponentAdded;
    entry.model_id = model_id;
    entry.component_id = component_id;
    recordStructural(std::move(entry));
}

void UndoStack::onComponentRemoving(const ComponentData& component)
{
    if (applying_)
        return;
    StructuralEntry entry;
    entry.kind = StructuralEntry::Kind::ComponentRemoved;
    if (auto op = model_->getComponentOperator(component.id))
        entry.model_id = op->modelId();
    entry.component_id = component.id;
    entry.component_snapshot = component.clone();
    recordStructural(std::move(entry));
}

void UndoStack::setOnChanged(std::function<void()> callback)
{
    on_changed_ = std::move(callback);
}

void UndoStack::pushRecord(UndoRecord record)
{
    if (record.empty())
        return;
    while (undo_.size() >= kMaxDepth)
        undo_.pop_front(); // 溢出丢最旧
    undo_.push_back(std::move(record));
    redo_.clear();
    if (on_changed_)
        on_changed_();
}

void UndoStack::recordStructural(StructuralEntry entry)
{
    // 边界内（如算法调用内 io.read 导入新模型）：并入当前操作，随边界 commit 成一条
    if (op_depth_ > 0) {
        op_structural_.push_back(std::move(entry));
        return;
    }
    // 边界外独立结构操作（如 QML 导入）：即时自成记录
    UndoRecord record;
    switch (entry.kind) {
    case StructuralEntry::Kind::ModelAdded: record.label = "添加模型"; break;
    case StructuralEntry::Kind::ModelRemoved: record.label = "删除模型"; break;
    case StructuralEntry::Kind::ComponentAdded: record.label = "添加组件"; break;
    case StructuralEntry::Kind::ComponentRemoved: record.label = "删除组件"; break;
    }
    record.structural.push_back(std::move(entry));
    pushRecord(std::move(record));
}

bool UndoStack::restoreComponentSnapshot(Index component_id, const ComponentData& snapshot)
{
    auto op = model_->getComponentOperator(component_id);
    if (!op)
        return false;
    op->restoreSnapshot(snapshot);
    return true;
}
