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
 * @param label 当前阶段或执行反馈（展示给用户，空串沿用上一句文字）
 *
 * Runner 在计算与 GUI 提交段提供有效回调，无展示方也可直接调用，无需判空。
 * 计算段每次上报是取消心跳；提交段只更新展示，不中断已经开始的提交。
 * 回调只在当前执行段内使用，不跨任务保存。同步算法的默认回调为空操作。
 */
using ProgressFn = std::function<void(double value, const std::string& label)>;
}
