/**
 * @file FeatureSystem.h
 * @brief 功能系统：功能的注册、调用与事件路由
 */
#ifndef FEATURE_SYSTEM_H
#define FEATURE_SYSTEM_H
#include "Core.h"
#include "EventBus.h"
#include "FeatureContext.h"
#include "FeatureEventGateway.h"
#include "FeatureEvents.h"
#include "FeatureInfo.h"
#include "FeatureParams.h"
#include "InteractionContext.h"
#include "InteractionState.h"
#include "SystemHandlerPtr.h"

#include <any>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class ModelLayer;
class UndoStack;

namespace systems::job {
class JobRunner;
struct JobWork;
}

namespace core {
class ArgObject;
}

namespace systems::feature {
class FeatureHandler;

struct HandlerMetaData {
    std::string name { }; //> 功能唯一名称，用作索引
    std::string display_name { }; //> 功能 UI 展示用名称
    std::string description { }; //> 功能描述
    bool interactive { }; //> 是否声明视口交互能力（功能经 interaction 上下文订阅交互回调）
};

/**
 * @brief 功能系统：管理功能插件的注册、注销、调用与事件路由
 *
 * 非单例，由 Session 组合持有。与算法/编辑系统不同，功能在注册后即常驻，
 * 可持续响应按键、参数变更与模型事件；并随活动操作切换被反复
 * 进入 activate() / 退出 deactivate()（见 setFeatureActive）。
 */
class FeatureSystem {
public:
    using SystemHandler = FeatureHandler; //> 功能处理器基类类型
    using SystemHandlerPtr = ::systems::SystemHandlerPtr<SystemHandler>; //> 兼容跨 dll 边界析构的智能指针
    static const std::string name; //> 系统唯一名称，用于插件注册时的识别

    FeatureSystem(ModelLayer& model_layer, core::EventBus& event_bus, UndoStack* undo_stack = nullptr);
    ~FeatureSystem();

    /**
     * @brief 空闲时注册功能：装配上下文 → setup 收集声明 → 载入参数默认值；占用期拒绝
     */
    bool registerHandler(const HandlerMetaData& meta_data, SystemHandlerPtr handler);
    /**
     * @brief 同步注销功能：占用期拒绝；空闲时当前功能先退出，teardown 后移除
     */
    void unregisterHandler(const HandlerMetaData& meta_data);
    /**
     * @brief 菜单触发的功能调用
     * @return 功能返回的结果，功能不存在时为空 std::any
     */
    std::any invoke(const std::string& unique_name);

    /**
     * @brief 修改功能参数并广播 ParameterChangedEvent
     * @return 功能与参数下标均有效时返回 true
     */
    bool setParameter(const std::string& unique_name, std::size_t index, core::ArgObject value);

    /**
     * @brief 派发按键事件：先向事件总线广播原始事件流，再做按键绑定路由
     * @return 任一绑定功能的 onKeyEvent 返回 true（事件已被消费）时为 true，
     *         调用方（UI 层）应据此 accept 事件、阻止其继续传递
     */
    bool dispatchKeyEvent(const KeyEvent& event);

    /**
     * @brief 获取已注册功能信息列表
     */
    std::vector<FeatureInfo*> getFeatureInfos();
    /** @brief 设置宿主始终保留的父入口；仅在模型操作空闲时允许变更。 */
    void setNavigationEntries(std::vector<FeatureNavigationEntry> entries);
    /** @brief 聚合宿主与功能声明的入口；相同 id 按提供者身份确定来源。 */
    std::vector<FeatureNavigationEntry> getNavigationEntries() const;
    /**
     * @brief 获取功能的参数集（UI 展示当前值用），功能不存在时为 nullptr
     */
    const FeatureParams* params(const std::string& unique_name) const;
    /**
     * @brief 获取当前激活的视口交互状态（渲染层据此路由拾取/悬停）
     * @return 声明 interactive 且 active 的功能状态；无激活交互时为 nullptr
     */
    interaction::InteractionState* activeInteraction();
    /**
     * @brief 切换当前进入的功能（活动操作切换驱动，所有功能可感知进入/退出）
     *
     * 退出旧功能（定序：先功能回调后扳交互开关）：先 handler->deactivate()，
     * 再 interactive 则下线其交互（功能在 deactivate() 中经 deferRefresh 挂的
     * 渲染线程清理由此保证在下线迁移时被消费）；
     * 进入新功能（同一定序）：先 handler->activate(ctx)，再 interactive 则上线其交互。
     * 任务占用期只保留最后一个有效目标，退出自己的任务后由框架应用；
     * 清理回调中的切换同样延后，避免重入销毁条目。无待处理请求时同名空转。
     * @param unique_name 要进入的功能唯一名称；空串表示退出当前功能（活动操作无功能身份）
     * @return 目标有效且已应用或已登记时为 true；非法目标或无法延后清理时为 false
     */
    bool setFeatureActive(const std::string& unique_name);
    /**
     * @brief 设置功能信息变更回调函数
     */
    void setOnFeatureInfosChanged(std::function<void()> callback);

    /**
     * @brief 注入动态上下文 provider（app 层调用，功能注册前后设置均生效）
     */
    void setActiveModelProvider(std::function<std::optional<Index>()> provider);
    void setActiveComponentProvider(std::function<std::optional<Index>()> provider);
    /**
     * @brief 设置视口渲染刷新回调（app 层注入，功能经 InteractionContext::requestRefresh 触发）
     */
    void setRenderRefreshCallback(std::function<void()> callback);
    /**
     * @brief 注入全局单槽任务执行器（Session 装配；验证同一模型宿主，未注入时发布返回空）
     */
    void setJobRunner(systems::job::JobRunner* runner);
    /**
     * @brief 重放占用期延后的模型事件（Session 终态流程在所属线程调用）
     *
     * 所有任务占用期间到达的模型事件回调暂存于此；
     * 重放时回调只访问自己的预览并 flush，不维护 undo 历史。单个重放回调异常记日志吞掉，
     * 不向调用方传播（发布者已错过，且重放位于 GUI 事件处理栈内）。
     */
    void flushDeferredEvents();

private:
    friend struct FeatureContext;
    friend class UndoContext;
    friend class InteractionContext;
    struct FeatureEntry {
        //! @brief 固定服务就地构造，内部引用不随容器扩容失效。
        FeatureEntry(FeatureSystem& system, const HandlerMetaData& meta_data);
        FeatureEntry(const FeatureEntry&) = delete;
        FeatureEntry& operator=(const FeatureEntry&) = delete;
        FeatureEntry(FeatureEntry&&) = delete;
        FeatureEntry& operator=(FeatureEntry&&) = delete;

        SystemHandlerPtr handler;
        FeatureInfo info;
        FeatureParams params;
        interaction::InteractionState interaction_state; //> 视口交互状态（回调 + 标注 + 结果）
        InteractionContext interaction_context; //> 固定绑定系统与本条目状态
        FeatureEventGateway event_gateway; //> 事件网关视图（构造时绑定功能所有者）
        FeatureContext context;
    };

    void cancelPendingJobs(const std::string& owner);
    //! @brief 在可写清理段退出实际当前功能，切换、注销和析构共用。
    void exitCurrentFeature();
    //! @brief 应用已验证的目标；不登记请求，不重入公开切换入口。
    void applyFeatureActive(const std::string& unique_name);
    //! @brief 单激活关系按状态身份判定，不在各功能复制转发闭包。
    void deactivateOtherInteractions(const interaction::InteractionState& state);
    //! @brief 固定交互服务读取当前宿主回调，晚装配与替换立即生效。
    void notifyRenderRefresh();
    std::shared_ptr<systems::job::Job> submitWriteJob(std::string label, const std::string& owner,
        std::function<systems::job::JobWork()> prepare,
        std::optional<std::uint64_t> preview_scope = std::nullopt, bool masked = false);

    bool routeKeyEvent(const KeyEvent& event); //> 按键绑定路由，返回事件是否被消费
    /**
     * @brief 纯计算任务提交（FeatureContext::runComputeJob 的实现）：无执行器或单槽忙时返回空
     */
    std::shared_ptr<systems::job::Job> submitComputeJob(std::string name, systems::job::JobTaskFn task, const std::string& owner);
    //! @brief 类型化入口的内部目标捕获与回写装配。
    std::shared_ptr<systems::job::Job> submitCapturedWriteback(std::string label, Index component_id,
        CaptureJobFn capture, WritebackFn write, const std::string& owner, bool masked);
    std::shared_ptr<systems::job::Job> submitCapturedWriteback(std::string label,
        LayerCaptureJobFn capture, LayerWritebackFn write, const std::string& owner, bool masked);
    //! @brief 影子组件任务提交（FeatureContext::runComponentJob 的实现）：影子 copy-in → worker → GUI 提交
    std::shared_ptr<systems::job::Job> submitComponentJob(std::string label, Index component_id, ComponentJobTaskFn task, const std::string& owner);
    //! @brief 生命周期清理边界（deactivate/teardown）：回调返回后统一 flush（异常时先 flush 再重抛）；
    //! 不成 undo 记录——退出清理（如删除功能生成的属性）若可撤销，撤销后会留下功能已停止跟踪的游离状态
    void flushAfterCallback(const std::function<void()>& fn);

    //! @brief 将模型清理留到任务实际结束，由框架持写权执行。
    bool deferCleanup(std::function<void()> cleanup);

    //! @brief 冻结期暂存事件回调（仅事件回调线程/GUI 调用；经网关注入）
    void deferEvent(std::function<void()> replay);

    std::shared_ptr<int> lifetime_ { std::make_shared<int>(0) }; //!< 延后清理的系统存活凭证
    ModelLayer* model_layer_; //< 模型层引用，用于装配功能上下文
    core::EventBus* event_bus_; //< 事件总线引用
    UndoStack* undo_stack_ { nullptr }; //< undo 栈引用（可空：无栈时边界退化为仅 flush）
    std::unordered_map<std::string, FeatureEntry> entries_; //< 功能条目，key 为功能唯一名称
    std::vector<FeatureNavigationEntry> navigation_entries_; //!< 宿主声明的常驻父入口，不含业务分类假设
    std::optional<std::string> pending_feature_; //!< 占用期最后一个有效目标；空字符串表示退出
    std::string current_feature_; //< 当前进入的功能名（空=无；setFeatureActive 驱动进入/退出）
    std::function<std::optional<Index>()> active_model_provider_;
    std::function<std::optional<Index>()> active_component_provider_;
    std::function<void()> on_feature_infos_changed_;
    std::function<void()> render_refresh_callback_; //> app 层注入：通知渲染窗口拉取标注并重绘
    systems::job::JobRunner* job_runner_ { nullptr }; //< 全局单槽执行器（Session 注入，不持有）
    std::vector<std::function<void()>> deferred_events_; //!< 冻结期暂存的事件回调重放闭包
};
}
#endif // FEATURE_SYSTEM_H
