/**
 * @file FeatureContext.h
 * @brief 功能上下文：功能访问软件环境的入口
 */
#ifndef FEATURE_CONTEXT_H
#define FEATURE_CONTEXT_H
#include "ComponentOperator.h" // std::optional<ComponentOperator> 需要完整类型
#include "Core.h"
#include "FeatureEventGateway.h" // events 成员类型（功能经其订阅事件，调用点需完整类型）

#include <functional>
#include <optional>

class ModelLayer;

namespace systems::feature {
class FeatureParams;
class InteractionContext;

/**
 * @brief 功能上下文，由 FeatureSystem 装配并在 activate()/execute() 时传给功能
 *
 * 组合而非单例：依赖以引用或 std::function 动态获取函数注入（后者由 app 层提供），
 * 功能层不反向依赖 app 层。当前功能可修改的范围仅限模型层对象。
 *
 * @note activeModel/activeComponent 是对象树选中态的动态查询，只视作一种提示：
 *       不要强制要求用户执行功能前在对象树中选中 component；目标组件优先经
 *       `Selector` 类型参数（FeatureParams）让用户显式选择后解析。
 */
struct FeatureContext {
    ModelLayer& model; //> 模型层入口
    FeatureEventGateway& events; //> 事件网关，功能在 activate() 中订阅事件（回调返回后自动 flush 组件变更通知）
    FeatureParams& params; //> 本功能的持久参数集
    InteractionContext& interaction; //> 视口交互入口，功能在 activate() 中订阅拾取/悬停回调
    std::function<std::optional<Index>()> activeModel; //> 动态获取当前活动模型 id
    std::function<std::optional<Index>()> activeComponent; //> 动态获取当前活动组件 id
    std::function<std::optional<ComponentOperator>(Index component_id)> componentOperator; //> 申请组件操作句柄
};
}
#endif // FEATURE_CONTEXT_H
