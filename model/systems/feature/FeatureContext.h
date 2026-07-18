/**
 * @file FeatureContext.h
 * @brief 功能上下文：功能访问软件环境的入口
 */
#ifndef FEATURE_CONTEXT_H
#define FEATURE_CONTEXT_H
#include "ComponentOperator.h" // std::optional<ComponentOperator> 需要完整类型
#include "Core.h"

#include <functional>
#include <optional>

class ModelLayer;

namespace core {
class EventBus;
}

namespace systems::feature {
class FeatureParams;

/**
 * @brief 功能上下文，由 FeatureSystem 装配并在 activate()/execute() 时传给功能
 *
 * 组合而非单例：依赖以引用或 std::function 动态获取函数注入（后者由 app 层提供），
 * 功能层不反向依赖 app 层。当前功能可修改的范围仅限模型层对象。
 */
struct FeatureContext {
    ModelLayer& model; //> 模型层入口
    core::EventBus& events; //> 事件总线，功能在 activate() 中订阅事件
    FeatureParams& params; //> 本功能的持久参数集
    std::function<std::optional<Index>()> activeModel; //> 动态获取当前活动模型 id
    std::function<std::optional<Index>()> activeComponent; //> 动态获取当前活动组件 id
    std::function<std::optional<ComponentOperator>(Index component_id)> componentOperator; //> 申请组件操作句柄
};
}
#endif // FEATURE_CONTEXT_H
