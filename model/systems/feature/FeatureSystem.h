/**
 * @file FeatureSystem.h
 * @brief 功能系统：功能的注册、调用与事件路由
 */
#ifndef FEATURE_SYSTEM_H
#define FEATURE_SYSTEM_H
#include "Core.h"
#include "EventBus.h"
#include "FeatureContext.h"
#include "FeatureEvents.h"
#include "FeatureInfo.h"
#include "FeatureParams.h"
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
};

/**
 * @brief 功能系统：管理功能插件的注册、注销、调用与事件路由
 *
 * 非单例，由 app 层组合持有。与算法/编辑系统不同，功能在注册后即处于
 * 激活状态，可持续响应按键、参数变更与模型事件。
 */
class FeatureSystem {
public:
    using SystemHandler = FeatureHandler; //> 功能处理器基类类型
    using SystemHandlerPtr = ::systems::SystemHandlerPtr<SystemHandler>; //> 兼容跨 dll 边界析构的智能指针
    static const std::string name; //> 系统唯一名称，用于插件注册时的识别

    FeatureSystem(ModelLayer& model_layer, core::EventBus& event_bus);
    ~FeatureSystem();

    /**
     * @brief 注册功能处理器插件：setup() 收集声明 → 装配上下文 → activate()
     */
    bool registerHandler(const HandlerMetaData& meta_data, SystemHandlerPtr handler);
    /**
     * @brief 注销功能处理器插件：deactivate() 后移除
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
     * @brief 设置功能信息变更回调函数
     */
    void setOnFeatureInfosChanged(std::function<void()> callback);

    /**
     * @brief 注入动态上下文 provider（app 层调用，功能注册前后设置均生效）
     */
    void setActiveModelProvider(std::function<std::optional<Index>()> provider);
    void setActiveComponentProvider(std::function<std::optional<Index>()> provider);

private:
    struct FeatureEntry {
        SystemHandlerPtr handler;
        std::unique_ptr<FeatureInfo> info;
        std::unique_ptr<FeatureParams> params;
        std::unique_ptr<FeatureContext> context;
    };

    bool routeKeyEvent(const KeyEvent& event); //> 按键绑定路由，返回事件是否被消费

    ModelLayer* model_layer_; //< 模型层引用，用于装配功能上下文
    core::EventBus* event_bus_; //< 事件总线引用
    std::unordered_map<std::string, FeatureEntry> entries_; //< 功能条目，key 为功能唯一名称
    std::function<std::optional<Index>()> active_model_provider_;
    std::function<std::optional<Index>()> active_component_provider_;
    std::function<void()> on_feature_infos_changed_;
};
}
#endif // FEATURE_SYSTEM_H
