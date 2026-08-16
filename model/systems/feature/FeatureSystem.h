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

namespace core {
class ArgObject;
}

namespace systems::feature {
class FeatureHandler;

struct HandlerMetaData {
    std::string name {}; //> 功能唯一名称，用作索引
    std::string display_name {}; //> 功能 UI 展示用名称
    std::string description {}; //> 功能描述
    bool interactive {}; //> 是否声明视口交互能力（功能经 interaction 上下文订阅交互回调）
};

/**
 * @brief 功能系统：管理功能插件的注册、注销、调用与事件路由
 *
 * 非单例，由 app 层组合持有。与算法/编辑系统不同，功能在注册后即常驻，
 * 可持续响应按键、参数变更与模型事件；并随活动操作切换被反复
 * 进入 activate() / 退出 deactivate()（见 setFeatureActive）。
 */
class FeatureSystem {
public:
    using SystemHandler = FeatureHandler; //> 功能处理器基类类型
    using SystemHandlerPtr = ::systems::SystemHandlerPtr<SystemHandler>; //> 兼容跨 dll 边界析构的智能指针
    static const std::string name; //> 系统唯一名称，用于插件注册时的识别

    FeatureSystem(ModelLayer& model_layer, core::EventBus& event_bus);
    ~FeatureSystem();

    /**
     * @brief 注册功能处理器插件：装配上下文 → setup() 收集声明并订阅事件 → 载入参数默认值
     */
    bool registerHandler(const HandlerMetaData& meta_data, SystemHandlerPtr handler);
    /**
     * @brief 注销功能处理器插件：当前功能先 deactivate() 退出，teardown() 后移除
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
     * 启停为幂等的状态应用：与当前功能同名时直接返回。
     * @param unique_name 要进入的功能唯一名称；空串表示退出当前功能（活动操作无功能身份）
     * @return 名称为空，或功能存在时为 true；功能不存在为 false 且不改变现状
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

private:
    struct FeatureEntry {
        SystemHandlerPtr handler;
        std::unique_ptr<FeatureInfo> info;
        std::unique_ptr<FeatureParams> params;
        interaction::InteractionState interaction_state; //> 视口交互状态（回调 + 标注 + 结果）
        InteractionContext interaction_context { interaction_state }; //> 交互上下文（包装本条目状态）
        std::unique_ptr<FeatureContext> context;
    };

    bool routeKeyEvent(const KeyEvent& event); //> 按键绑定路由，返回事件是否被消费
    //! @brief 生命周期回调边界（activate/deactivate/teardown）：回调返回后统一 flush（异常时先 flush 再重抛）；
    //! 不成 undo 记录——退出清理（如删除功能生成的属性）若可撤销，撤销后会留下功能已停止跟踪的游离状态
    void flushAfterCallback(const std::function<void()>& fn);

    ModelLayer* model_layer_; //< 模型层引用，用于装配功能上下文
    core::EventBus* event_bus_; //< 事件总线引用
    FeatureEventGateway event_gateway_; //< 功能事件网关（回调边界自动 flush；声明顺序须在 model_layer_/event_bus_ 之后）
    std::unordered_map<std::string, FeatureEntry> entries_; //< 功能条目，key 为功能唯一名称
    std::string current_feature_; //< 当前进入的功能名（空=无；setFeatureActive 驱动进入/退出）
    std::function<std::optional<Index>()> active_model_provider_;
    std::function<std::optional<Index>()> active_component_provider_;
    std::function<void()> on_feature_infos_changed_;
    std::function<void()> render_refresh_callback_; //> app 层注入：通知渲染窗口拉取标注并重绘
};
}
#endif // FEATURE_SYSTEM_H
