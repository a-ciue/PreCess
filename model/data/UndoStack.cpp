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

#include <algorithm>
#include <iterator>
#include <spdlog/spdlog.h>
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

//! @brief 同组件保留最早前像与最后的非空后像；预览收尾前尚无后像。
void mergeComponentEntries(std::vector<ComponentEntry>& target, std::vector<ComponentEntry>&& source)
{
    for (auto& entry : source) {
        const auto found = std::find_if(target.begin(), target.end(), [&](const ComponentEntry& existing) {
            return existing.component_id == entry.component_id;
        });
        if (found == target.end())
            target.push_back(std::move(entry));
        else if (entry.after)
            found->after = std::move(entry.after);
    }
}

//! @brief 恢复动作按原操作与方向裁决，删除前更新快照供下一次恢复；调用方持 ApplyingGuard。
void applyStructuralEntry(ModelLayer& model, StructuralEntry& entry, bool undo)
{
    // 新增的逆操作与删除的正操作均为删除，其余为恢复；条目原身份不变。
    switch (entry.kind) {
    case StructuralEntry::Kind::ModelAdded:
    case StructuralEntry::Kind::ModelRemoved:
        if ((entry.kind == StructuralEntry::Kind::ModelAdded) == undo) {
            entry.model_snapshot = model.takeModelSnapshot(entry.model_id);
            model.removeModel(entry.model_id);
        } else
            model.restoreModel(*entry.model_snapshot);
        break;
    case StructuralEntry::Kind::ComponentAdded:
    case StructuralEntry::Kind::ComponentRemoved:
        if ((entry.kind == StructuralEntry::Kind::ComponentAdded) == undo) {
            if (ComponentData* component = model.findComponent(entry.component_id)) {
                entry.component_snapshot = component->clone();
                model.removeComponent(entry.component_id);
            }
        } else
            model.restoreComponent(entry.model_id, entry.component_snapshot->clone());
        break;
    }
}
}

UndoStack::UndoStack(ModelLayer& model)
    : model_(&model)
{
}

UndoStack::OwnerScope::OwnerScope(UndoStack* stack, std::string owner)
    : stack_(stack)
{
    if (!stack_)
        return;
    stack_->model_->assertOwnerThread();
    previous_ = std::move(stack_->owner_);
    stack_->owner_ = std::move(owner);
}

UndoStack::OwnerScope::~OwnerScope()
{
    if (stack_)
        stack_->owner_ = std::move(previous_);
}

// —— 操作边界（自动模式）——

UndoStack::PreviewAccess::PreviewAccess(UndoStack* stack)
    : stack_(stack)
{
    if (stack_)
        ++stack_->preview_access_depth_;
}

UndoStack::PreviewAccess::~PreviewAccess()
{
    if (stack_)
        --stack_->preview_access_depth_;
}

UndoStack::RecordingPause::RecordingPause(UndoStack* stack)
    : stack_(stack)
{
    if (!stack_)
        return;
    stack_->model_->assertOwnerThread();
    if (stack_->inPreviewCallback() || stack_->inOperation()
        || (stack_->model_->writesPending() && !stack_->model_->hasWritePrivilege()))
        throw ModelOperationBusy("UndoStack: unrecorded cleanup requires an idle framework write segment");
    ++stack_->recording_pause_depth_;
}
UndoStack::RecordingPause::~RecordingPause()
{
    if (stack_)
        --stack_->recording_pause_depth_;
}

std::uint64_t UndoStack::scopeId() const
{
    return preview_ && preview_->owner == owner_ ? preview_->scope_id : 0;
}

void UndoStack::prepareOperation()
{
    model_->assertOwnerThread();
    if (inPreviewCallback() || applying_)
        throw ModelOperationBusy("UndoStack: formal operation refused during callback or restore");
    if (model_->writesPending() && !model_->hasWritePrivilege())
        throw ModelOperationBusy("UndoStack: operation refused while model job is pending");
    // 同步子调用共享根操作，不重新处置预览。
    if (!operation_ && preview_)
        cancelScope();
}

void UndoStack::beginOperation(std::string label, bool finish_preview)
{
    model_->assertOwnerThread();
    if (inPreviewCallback() || applying_)
        throw ModelOperationBusy("UndoStack: undo operation refused during callback or restore");
    if (model_->writesPending() && !model_->hasWritePrivilege())
        throw ModelOperationBusy("UndoStack: operation refused while model job is pending");
    if (operation_) {
        ++operation_depth_;
        return;
    }
    if (!finish_preview || scopeId() == 0)
        prepareOperation();
    Capture capture;
    capture.label = std::move(label);
    capture.owner = owner_;
    capture.finish_preview = finish_preview;
    operation_ = std::move(capture);
    operation_depth_ = 1;
}

bool UndoStack::beginScope(std::string label)
{
    model_->assertOwnerThread();
    if (operation_ && operation_->owner != owner_)
        throw ModelOperationBusy("UndoStack: preview must belong to the outer operation owner");
    if (inPreviewCallback() && scopeActive() && scopeId() == 0)
        throw ModelOperationBusy("UndoStack: event cannot replace another owner's preview");
    if (model_->writesPending() && !model_->hasWritePrivilege())
        throw ModelOperationBusy("UndoStack: scope mutation refused while model job is pending");
    if (applying_)
        return false;
    cancelScope();
    Capture preview;
    preview.label = std::move(label);
    preview.owner = owner_;
    preview.scope_id = ++next_scope_id_;
    preview_ = std::move(preview);
    return true;
}

void UndoStack::commitOperation()
{
    model_->assertOwnerThread();
    if (inPreviewCallback())
        throw ModelOperationBusy("UndoStack: event callbacks cannot commit undo operations");
    if (!operation_ || --operation_depth_ != 0)
        return;
    Capture frame = std::move(*operation_);
    operation_.reset();
    if (frame.delegated)
        return;
    if (frame.empty() && frame.finish_preview && preview_ && frame.owner == preview_->owner)
        frame.label = preview_->label;
    absorbPreview(frame);
    if (!frame.empty()) {
        for (auto& entry : frame.components)
            if (ComponentData* c = model_->findComponent(entry.component_id))
                entry.after = c->clone();
        pushRecord(UndoRecord { .label = std::move(frame.label),
                       .components = std::move(frame.components),
                       .structural = std::move(frame.structural) },
            frame.owner);
    } else if (on_changed_) {
        on_changed_();
    }
}

void UndoStack::absorbPreview(Capture& capture)
{
    if (!capture.finish_preview || !preview_ || capture.owner != preview_->owner)
        return;
    mergeComponentEntries(capture.components, std::move(preview_->components));
    capture.structural.insert(capture.structural.end(),
        std::make_move_iterator(preview_->structural.begin()),
        std::make_move_iterator(preview_->structural.end()));
    preview_.reset();
}

UndoStack::Capture UndoStack::detachOperation()
{
    Capture capture = std::move(*operation_);
    absorbPreview(capture);
    operation_.emplace();
    operation_->delegated = true;
    return capture;
}

void UndoStack::resumeOperation(Capture capture)
{
    operation_ = std::move(capture);
    operation_depth_ = 0; // GUI 提交的 ModelScope 开启唯一同步写段。
}

void UndoStack::discardOperation(Capture& capture)
{
    {
        ApplyingGuard guard(applying_);
        rollbackChanges(capture.components, capture.structural);
    }
    model_->flushNotifications();
    if (on_changed_)
        on_changed_();
}

// —— 插件层（带回滚点的层，savepoint 语义）——

void UndoStack::cancelScope()
{
    restorePreview(true);
}

void UndoStack::revertScope()
{
    restorePreview(false);
}

void UndoStack::restorePreview(bool close)
{
    model_->assertOwnerThread();
    if (inPreviewCallback() && scopeActive() && scopeId() == 0)
        throw ModelOperationBusy("UndoStack: event cannot change another owner's preview");
    if (model_->writesPending() && !model_->hasWritePrivilege())
        throw ModelOperationBusy("UndoStack: scope mutation refused while model job is pending");
    if (!preview_)
        return;
    {
        ApplyingGuard guard(applying_);
        rollbackChanges(preview_->components, preview_->structural);
    }
    if (close)
        preview_.reset();
    else {
        // 已退回开层状态；保持身份并清空捕获，避免结构操作被再次回滚。
        preview_->components.clear();
        preview_->structural.clear();
    }
    model_->flushNotifications();
    if (on_changed_)
        on_changed_();
}

bool UndoStack::scopeActive() const
{
    return preview_.has_value();
}

// —— 功能会话（activate → deactivate 收尾折叠成一条）——

void UndoStack::beginSession(std::string owner, std::string label)
{
    model_->assertOwnerThread();
    if (inPreviewCallback())
        throw ModelOperationBusy("UndoStack: event callbacks cannot mutate undo history");
    if (owner.empty())
        return;
    // 上一个会话没收尾（异常路径漏调 endSession）：先折叠再接新的，避免两段混成一个标
    if (!session_owner_.empty() && session_owner_ != owner) {
        spdlog::warn("UndoStack::beginSession: previous session '{}' not closed, collapsing it first",
            session_owner_);
        endSession(session_owner_);
    }
    session_owner_ = std::move(owner);
    session_label_ = std::move(label);
}

void UndoStack::endSession(std::string owner)
{
    model_->assertOwnerThread();
    if (inPreviewCallback())
        throw ModelOperationBusy("UndoStack: event callbacks cannot mutate undo history");
    if (owner.empty() || owner != session_owner_) {
        spdlog::warn("UndoStack::endSession: owner '{}' does not match active session '{}', ignored",
            owner, session_owner_);
        return;
    }
    const std::string label = session_label_;
    session_owner_.clear();
    session_label_.clear();

    // 折叠栈顶连续的同会话标记录。只折叠连续段：会话中途夹了别人的记录（旁路操作）时，
    // 跨过去合并会把别人的改动卷进同一条前像，撤销会多撤——早先的会话记录保持独立。
    std::size_t first = undo_.size();
    while (first > 0 && undo_[first - 1].session == owner)
        --first;
    const std::size_t count = undo_.size() - first;
    if (count == 0)
        return; // 会话期没成过记录（或都被撤销了）
    if (count == 1) {
        undo_[first].session.clear(); // 已是一条：只清标（会话已收尾）
        return;
    }

    UndoRecord merged = std::move(undo_[first]);
    if (!label.empty())
        merged.label = label; // 折叠条目用会话名（功能显示名），不是某一次执行的名字
    for (std::size_t i = first + 1; i < undo_.size(); ++i) {
        auto& source = undo_[i];
        mergeComponentEntries(merged.components, std::move(source.components));
        merged.structural.insert(merged.structural.end(),
            std::make_move_iterator(source.structural.begin()),
            std::make_move_iterator(source.structural.end()));
    }
    merged.session.clear();
    undo_.erase(undo_.begin() + static_cast<std::ptrdiff_t>(first + 1), undo_.end());
    undo_[first] = std::move(merged);
    if (on_changed_)
        on_changed_();
}

void UndoStack::applyRecordUndo(UndoRecord record)
{
    {
        ApplyingGuard guard(applying_);
        rollbackChanges(record.components, record.structural);
    }
    model_->flushNotifications();

    redo_.push_back(std::move(record)); // 移入 redo：可 redo 做回来
    if (on_changed_)
        on_changed_();
}

bool UndoStack::hasBoundaryFrame() const
{
    return operation_.has_value();
}

void UndoStack::rollbackChanges(const std::vector<ComponentEntry>& components,
    std::vector<StructuralEntry>& structural)
{
    // 先逆序恢复结构，组件前像才有目标可落地；历史与预览恢复共用此顺序。
    rollbackStructural(structural);
    for (const auto& entry : components)
        if (entry.before)
            restoreComponentSnapshot(entry.component_id, *entry.before);
}

void UndoStack::rollbackStructural(std::vector<StructuralEntry>& entries)
{
    // 逆序回滚（后做的先撤）；回滚中重新捕获的快照存回条目，供记录路径的 redo 使用
    for (auto it = entries.rbegin(); it != entries.rend(); ++it)
        applyStructuralEntry(*model_, *it, true);
}

// —— 撤销/重做 ——

bool UndoStack::canUndo() const
{
    // 层打开：undo = cancelScope（回滚预览），有意义
    return scopeActive() || !undo_.empty();
}

bool UndoStack::canRedo() const
{
    // 层打开时 redo 空转（无环节概念）
    if (scopeActive())
        return false;
    return !redo_.empty();
}

std::optional<std::string> UndoStack::undoLabel() const
{
    if (preview_)
        return preview_->label;
    if (undo_.empty())
        return std::nullopt;
    return undo_.back().label;
}

std::optional<std::string> UndoStack::redoLabel() const
{
    if (scopeActive())
        return std::nullopt;
    if (redo_.empty())
        return std::nullopt;
    return redo_.back().label;
}

bool UndoStack::undo()
{
    model_->assertOwnerThread();
    // 模型操作占用守卫（先于一切栈状态变更）
    if (!prepareMutation("undo"))
        return false;

    // 层打开：undo = cancelScope（按层内捕获回滚并关闭层，消费本次），全局栈记录不动
    if (scopeActive()) {
        cancelScope();
        return true;
    }

    if (undo_.empty())
        return false;
    UndoRecord record = std::move(undo_.back());
    undo_.pop_back();
    applyRecordUndo(std::move(record));
    return true;
}

bool UndoStack::redo()
{
    model_->assertOwnerThread();
    // 模型操作占用守卫（先于一切栈状态变更）
    if (!prepareMutation("redo"))
        return false;

    // 层打开：redo 空转（无环节概念）
    if (scopeActive())
        return false;

    if (redo_.empty())
        return false;
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
        for (auto& entry : record.structural)
            applyStructuralEntry(*model_, entry, false);
    }
    model_->flushNotifications();

    undo_.push_back(std::move(record));
    if (on_changed_)
        on_changed_();
    return true;
}

void UndoStack::clear()
{
    model_->assertOwnerThread();
    if (inPreviewCallback())
        throw ModelOperationBusy("UndoStack: event callbacks cannot mutate undo history");
    if (model_->writesPending() && !model_->hasWritePrivilege())
        throw ModelOperationBusy("UndoStack: clearing history refused while a model operation is pending");
    undo_.clear();
    redo_.clear();
    if (on_changed_)
        on_changed_();
}

// —— UndoRecorder 钩子 ——

void UndoStack::onComponentDirty(Index component_id, const ComponentData& data)
{
    // 恢复不记账；其余首次写捕前像，钩子不改变模型或捕获归属。
    if (applying_ || recording_pause_depth_)
        return;
    if (operation_)
        operation_->writes_started = true;

    Capture* target = captureTarget();
    if (!target)
        return; // 无层无边界：无归属直写到不了此处（写前已拒）
    for (const auto& entry : target->components) {
        if (entry.component_id == component_id)
            return;
    }
    target->components.push_back(ComponentEntry { component_id, data.clone(), nullptr });
}

bool UndoStack::allowsUnrecordedWrite() const
{
    if (inPreviewCallback() && !applying_)
        return scopeId() != 0;
    // 三种有归属的写路径：操作边界内（自动记账）、恢复中（自身即账务）、层自写（层持其捕获）
    return hasBoundaryFrame() || applying_ || recording_pause_depth_ || scopeActive();
}

void UndoStack::onModelAdded(Index model_id)
{
    if (applying_ || recording_pause_depth_)
        return;
    StructuralEntry entry;
    entry.kind = StructuralEntry::Kind::ModelAdded;
    entry.model_id = model_id;
    recordStructural(std::move(entry));
}

void UndoStack::onModelRemoving(const ModelData& model)
{
    if (applying_ || recording_pause_depth_)
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
    if (applying_ || recording_pause_depth_)
        return;
    StructuralEntry entry;
    entry.kind = StructuralEntry::Kind::ComponentAdded;
    entry.model_id = model_id;
    entry.component_id = component_id;
    recordStructural(std::move(entry));
}

void UndoStack::onComponentRemoving(const ComponentData& component)
{
    if (applying_ || recording_pause_depth_)
        return;
    const Index component_id = component.id;
    StructuralEntry entry;
    entry.kind = StructuralEntry::Kind::ComponentRemoved;
    if (auto op = model_->getComponentOperator(component_id))
        entry.model_id = op->modelId();
    entry.component_id = component_id;
    entry.component_snapshot = component.clone(); // 写前捕获，引用仍有效
    recordStructural(std::move(entry));
}

void UndoStack::setOnChanged(std::function<void()> callback)
{
    on_changed_ = std::move(callback);
}

bool UndoStack::hasPendingOperationWrites() const
{
    return operation_ && (!operation_->empty() || (operation_->finish_preview && preview_ && !preview_->empty()));
}

//! undo/redo 共同入口：临界区拒绝优先（零副作用），任务占用期同样零副作用拒绝
bool UndoStack::prepareMutation(const char* op) const
{
    if (inPreviewCallback())
        return false;
    // 守卫：撤销/重做永不进临界区——边界内（记录/兑现中）或恢复中
    // （applying_，重入插队）直接拒绝；拒绝零副作用：不触发任务取消、不动栈不动模型
    if (hasBoundaryFrame()) {
        spdlog::warn("UndoStack::{}: refused inside operation boundary '{}' (frames {}) — "
                     "undo/redo must not enter a critical section",
            op, operation_->label, operation_depth_);
        return false;
    }
    if (applying_) {
        spdlog::warn("UndoStack::{}: refused during restore (applying_) — reentrant mutation",
            op);
        return false;
    }
    if (model_ && model_->writesPending()) {
        spdlog::warn("UndoStack::{}: model operation pending, refused; retry after the task stops",
            op);
        return false;
    }
    return true;
}

void UndoStack::pushRecord(UndoRecord record, const std::string& owner)
{
    if (record.empty())
        return;
    // 会话标：会话开着且这条记录是会话所有者自己成的 → 打标（endSession 收尾折叠的判据）
    if (!session_owner_.empty() && owner == session_owner_)
        record.session = session_owner_;
    while (undo_.size() >= kMaxDepth)
        undo_.pop_front(); // 溢出丢最旧
    undo_.push_back(std::move(record));
    redo_.clear();
    if (on_changed_)
        on_changed_();
}

UndoStack::Capture* UndoStack::captureTarget()
{
    return preview_ ? &*preview_ : (operation_ ? &*operation_ : nullptr);
}

void UndoStack::recordStructural(StructuralEntry entry)
{
    if (operation_)
        operation_->writes_started = true;
    Capture* target = captureTarget();
    if (target) {
        target->structural.push_back(std::move(entry));
        return;
    }
    throw std::logic_error("UndoStack: structural write requires an explicit operation boundary");
}

bool UndoStack::restoreComponentSnapshot(Index component_id, const ComponentData& snapshot)
{
    auto op = model_->getComponentOperator(component_id);
    if (!op)
        return false;
    op->restoreSnapshot(snapshot);
    return true;
}
