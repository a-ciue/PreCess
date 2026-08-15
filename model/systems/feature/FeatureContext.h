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
#include <string>

class ModelLayer;

namespace systems::feature {
class FeatureParams;
class InteractionContext;

/**
 * @brief 功能的 undo 上下文（轻量视图，包装 UndoStack + 本功能的 undo 模式）
 *
 * Manual 功能（json 声明 "undo": "manual"）经 staged 会话显式控制记录：
 * beginStaged（栈捕获 before₀）→ editableMesh 预览写（重试经 revertStaged 回滚再改）→
 * 确认 commitStaged（before₀+当前状态成一条记录）/ 取消 cancelStaged（恢复 before₀ 不成记录）。
 * 预览写的标脏在 Manual 边界只 flush 不成记录；before-image 由栈持有（非插件自持）。
 * Auto 功能由操作边界自动记录，不使用本上下文的 staged 接口。
 */
struct UndoContext {
    std::function<bool(std::string label, Index component_id)> beginStaged; //!< 开启 staged 会话（栈捕获 before₀）
    std::function<void()> commitStaged; //!< 确认：before₀+当前状态成一条记录
    std::function<void()> cancelStaged; //!< 恢复 before₀ 并关闭会话，不成记录
    std::function<void()> revertStaged; //!< 恢复 before₀，会话保持打开（预览重试）
    std::function<bool()> stagedActive; //!< 是否有进行中的 staged 会话
    bool automatic { true }; //!< 本功能是否自动模式（只读）
};

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
    UndoContext undo; //> undo 上下文（Manual 功能的 staged 会话入口；语义见 UndoContext 注释）
};
}
#endif // FEATURE_CONTEXT_H
