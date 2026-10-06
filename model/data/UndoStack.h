/**
 * @file UndoStack.h
 * @brief 一个正式历史栈与临时预览捕获：事件不入栈，execute 收尾吸收预览。
 *
 * 正式边界首次写捕 before-image，收尾补 after-image，空操作丢弃。
 * 预览层可跨事件存活，revertScope 回起点并保持层，cancelScope 回滚关闭。
 * 同所有者 execute 继续预览捕获，收尾吸收并关闭层；其他正式操作抢占时回滚层。
 * 事件与后台预览回写只能写自己的层，不借用正式边界或修改历史。
 * 功能会话只给正式记录打标，deactivate 折叠栈顶连续同标项，不跨过其他操作。
 * 任务占用期拒绝 undo/redo 与非授权写；取消不提前释放占用。
 */
#pragma once
#include "Core.h"
#include "UndoRecord.h"
#include "UndoRecorder.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class ModelLayer;
namespace systems::job {
class JobRunner;
}

class UndoStack : public UndoRecorder {
public:
    //! @brief 栈深度上限（before+after 双快照，大网格内存受控），溢出丢最旧
    static constexpr std::size_t kMaxDepth = 32;

    explicit UndoStack(ModelLayer& model);
    //! @brief 栈绑定模型且独占撤销快照，不可复制。
    UndoStack(const UndoStack&) = delete;
    UndoStack& operator=(const UndoStack&) = delete;

    /**
     * @brief 所有者上下文守卫：功能入口（invoke / 按键路由 / 事件回调）进入时压入功能
     *        唯一名，析构弹出——嵌套安全（各功能的入口可相互重入）
     *
     * 预览与正式记录按此标识归属；execute 只吸收自己的预览。stack 为空时为空操作。
     */
    class OwnerScope {
    public:
        OwnerScope(UndoStack* stack, std::string owner);
        ~OwnerScope();
        OwnerScope(const OwnerScope&) = delete;
        OwnerScope& operator=(const OwnerScope&) = delete;

    private:
        UndoStack* stack_ { nullptr };
        std::string previous_; //!< 压入前的外层所有者（析构还原）
    };

    /** @brief 事件与预览回写访问：只允许写自己的临时层，不允许修改正式历史。 */
    class PreviewAccess {
    public:
        explicit PreviewAccess(UndoStack* stack);
        ~PreviewAccess();
        PreviewAccess(const PreviewAccess&) = delete;
        PreviewAccess& operator=(const PreviewAccess&) = delete;

    private:
        UndoStack* stack_;
    };
    //! @brief 当前所有者的预览身份；没有自己的层时为零。
    std::uint64_t scopeId() const;
    bool inPreviewCallback() const { return preview_access_depth_ != 0; }

    /** @brief 框架生命周期清理／初始化：不生成历史，仍受线程和在飞写权限制。 */
    class RecordingPause {
    public:
        explicit RecordingPause(UndoStack* stack);
        ~RecordingPause();
        RecordingPause(const RecordingPause&) = delete;
        RecordingPause& operator=(const RecordingPause&) = delete;

    private:
        UndoStack* stack_;
    };

    // —— 操作边界（自动模式）——
    /**
     * @brief 开正式边界；普通入口先回滚旧预览，同 owner execute 保留并吸收预览。
     * @param finish_preview execute 收尾吸收并关闭临时层；普通事件禁止开边界。
     */
    void beginOperation(std::string label, bool finish_preview = false);
    //! @brief 正式入口准备：在解析目标或拷贝模型前回滚旧预览；execute 内同步嵌套保留本层。
    void prepareOperation();
    //! @brief 深度-1；归零时：空操作丢弃，否则补 after-image、入栈、清空 redo
    void commitOperation();

    // —— 插件预览（开层、回滚保持、回滚关闭；execute 自动确认）——
    /**
     * @brief 开启唯一预览捕获（插件可 cancelScope / revertScope）
     *
     * 层不指定目标组件：层开着期间的组件写与结构操作都归层，回滚覆盖层内全部捕获。
     * 已有未完成的层时不拒绝：新请求 = 旧层被放弃的证明，先驱逐旧层（回滚关闭）再开
     * 新层——预览层是便宜的（改动从未确认，回滚即净），不必等新层的第一次真实改动
     * 来间接驱逐。
     * @return false = 恢复中（applying_）被拒，预览保持不变
     */
    bool beginScope(std::string label);
    //! @brief 按层内捕获回滚（结构条目逆序 → 组件 before-image）并关闭层，不成记录（不产生栈项——不受最外层规则限制）；无层空转
    void cancelScope();
    //! @brief 按层内捕获回滚，层保持打开（纯预览重试；捕获清空，后续改动重新捕获）；无层空转
    void revertScope();
    //! @brief 是否有进行中的预览
    bool scopeActive() const;

    // —— 功能会话（activate → deactivate 期间的记录，收尾折叠成一条）——
    /**
     * @brief 开始功能会话：此后**所有者 = owner** 成的记录打上会话标
     *
     * 会话**不是帧**——不进帧栈，因此撤销守卫、模态事件循环规则、层的归属判定全部不受
     * 影响（会话跨任意多次事件轮次是设计使然）。粒度设计：会话期内每条记录各自独立
     * （每次 execute 一步、可逐步撤销），`endSession` 收尾时折叠——一个功能会话 = 一条撤销条目。
     * @param owner 功能唯一名（与功能入口压入的所有者一致）
     * @param label 折叠后那条记录的名字（功能显示名）
     */
    void beginSession(std::string owner, std::string label);
    /**
     * @brief 结束功能会话：把**栈顶连续**的同会话标记录折叠成一条（名字 = beginSession 的 label）
     *
     * 只折叠栈顶连续段：会话中途夹了别人的记录（旁路操作）时，早先的会话记录保持独立——
     * 跨过别人的记录合并会把别人的改动卷进同一条前像，撤销时会多撤。
     * owner 与进行中会话不符 → warn 空转；无会话标记录 → 空转。
     */
    void endSession(std::string owner);
    //! @brief 当前所有者上下文（功能入口压入的名字；空 = 无归属）——任务提交闭包延迟打标用
    const std::string& currentOwner() const noexcept { return owner_; }

    // —— 撤销/重做（自身即边界：恢复后 flushNotifications）——
    //! @brief 正式边界是否开着（框架提交门查询，同线程读）。
    bool inOperation() const noexcept { return hasBoundaryFrame(); }
    //! @brief 当前未收尾的边界是否已写模型；异步任务只能从尚未写入的入口发布。
    bool hasPendingOperationWrites() const;
    bool canUndo() const;
    bool canRedo() const;
    std::optional<std::string> undoLabel() const;
    std::optional<std::string> redoLabel() const;
    //! @brief 层打开 = cancelScope（按捕获回滚并关闭，消费本次，全局栈不动）；否则正常撤销
    //! @return true = 已恢复模型；忙、空栈或边界内拒绝时返回 false。
    bool undo();
    //! @brief 层打开空转；否则正常重做
    //! @return true = 已重做；忙、空栈或层会话期拒绝时返回 false。
    bool redo();
    void clear();

    // —— UndoRecorder 钩子（ModelLayer 回调）——
    void onComponentDirty(Index component_id, const ComponentData& data) override; //!< 首次写捕前像；事件归自己的预览，正式写归当前捕获
    //! @brief 写前归属判定：边界内 / 恢复中 / 层自写三者有其一即放行，否则写前拒绝（接口默认放行，本实现收紧）
    bool allowsUnrecordedWrite() const override;
    // 结构修改与组件数据写使用相同捕获归属。
    void onModelAdded(Index model_id) override;
    void onModelRemoving(const ModelData& model) override; //!< removeModel 前捕获 ModelSnapshot
    void onComponentAdded(Index model_id, Index component_id) override;
    void onComponentRemoving(const ComponentData& component) override; //!< removeComponent 前克隆

    //! @brief 设置栈内容变更回调（入栈/撤销/重做/清空/层状态变化时触发；app 层 QML 刷新用）
    void setOnChanged(std::function<void()> callback);

private:
    friend class systems::job::JobRunner;
    /** @brief 正式与预览使用同一种捕获数据，各自只保留一个实例。 */
    struct Capture {
        std::string label;
        std::string owner; //!< 开帧时的所有者（功能唯一名；空 = 无归属：框架/QML/裸调用）——预览抢占的判据
        bool finish_preview { false }; //!< execute 收尾吸收并关闭临时层
        bool writes_started { false }; //!< 本次同步边界实际写入，不含进入前的旧预览
        bool delegated { false }; //!< 捕获已交给任务；同步作用域只退出深度，不入栈
        std::uint64_t scope_id { 0 }; //!< 层身份，迟到回写不得借用后来新开的层
        std::vector<ComponentEntry> components; //!< 本帧捕获的组件写（first-dirty before + 收尾 after）
        std::vector<StructuralEntry> structural; //!< 本帧结构操作（层帧内 = 层的捕获，随层回滚/提交）
        bool empty() const { return components.empty() && structural.empty(); }
    };

    //! @brief 后台续接使用现有捕获；只由唯一 Runner 在模型占用期调用。
    Capture detachOperation();
    void resumeOperation(Capture capture);
    void discardOperation(Capture& capture);
    void absorbPreview(Capture& capture);

    //! @brief 回滚本预览，关闭或清空捕获保持同一身份；不产生历史或 redo。
    void restorePreview(bool close);
    //! @brief 结构逆序 → 组件 before-image；调用方须持 ApplyingGuard。
    void rollbackChanges(const std::vector<ComponentEntry>& components, std::vector<StructuralEntry>& structural);
    //! @brief 结构条目逆序回滚（undo 与层回滚共用；回滚中重新捕获的快照存回条目供 redo 用）
    void rollbackStructural(std::vector<StructuralEntry>& entries);
    //! @brief 写钩子的共同目标：预览优先，否则正式捕获；无归属返回 nullptr。
    Capture* captureTarget();
    //! @brief 是否存在正式操作边界
    bool hasBoundaryFrame() const;

    //! @brief 入栈（超 kMaxDepth 丢最旧）、清空 redo、触发变更回调；owner 匹配进行中会话时给记录打会话标
    void pushRecord(UndoRecord record, const std::string& owner);
    //! @brief 结构条目只归预览或正式捕获；无边界结构写由模型层预先拒绝。
    void recordStructural(StructuralEntry entry);
    //! @brief 把弹出的记录兑现成撤销：结构条目逆序 → 组件 before-image → 移入 redo
    void applyRecordUndo(UndoRecord record);
    /**
     * @brief undo/redo 共同入口守卫（临界区拒绝优先于一切副作用）：
     *        边界内（operation_depth_>0，记录/兑现中）或恢复中（applying_）直接拒绝——零副作用
     *        （不触发任务取消、不动栈不动模型）；任务占用期返回 false；拒绝不取消任务、不改变模型或栈
     */
    bool prepareMutation(const char* op) const;
    //! @brief 组件存在则恢复快照（gid 对账内建于 restoreSnapshot），返回是否恢复
    bool restoreComponentSnapshot(Index component_id, const ComponentData& snapshot);

    //! @brief 集成测试只读检查历史条目数量，验证事件不入栈。
    friend struct UndoStackTestPeer;

    ModelLayer* model_;
    unsigned preview_access_depth_ { 0 };
    unsigned recording_pause_depth_ { 0 };
    std::uint64_t next_scope_id_ { 0 };
    bool applying_ { false }; //!< undo/redo/cancelScope 应用中：抑制一切记录（恢复会回触钩子）
    std::string owner_; //!< 当前所有者上下文（OwnerScope 压入/还原）；开层、开边界时打到帧上
    std::string session_owner_; //!< 进行中的功能会话所有者（空 = 无会话）——记录打标判据
    std::string session_label_; //!< 会话收尾折叠后那条记录的名字（功能显示名）
    std::optional<Capture> operation_; //!< 唯一正式捕获，子调用共用
    unsigned operation_depth_ { 0 }; //!< 同步嵌套深度，不跨事件轮次
    std::optional<Capture> preview_; //!< 唯一预览捕获，独立于正式边界
    std::deque<UndoRecord> undo_, redo_;
    std::function<void()> on_changed_;
};
