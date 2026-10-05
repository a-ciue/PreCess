/**
 * @file JobProgress.h
 * @brief 进度上报回调类型：model 层（任务系统）与上层 UI 适配层的公共契约
 */
#pragma once
#include <functional>
#include <string>

namespace systems::job {
/**
 * @brief 进度上报回调
 * @param value 完成度，取值 0~1
 * @param label 当前阶段描述（展示给用户，可为空串）
 *
 * 上下文（HandlerContext / FeatureContext）提供 no-op 默认实现，任务可无条件调用、无需判空；
 * 由上层（如 QML 适配器）注入真实实现：更新进度属性并驱动界面刷新。
 * 心跳语义：任务执行期间回调被调用即视为心跳（取消检查在此进行）。
 */
using ProgressFn = std::function<void(double value, const std::string& label)>;
}
