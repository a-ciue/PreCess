/**
 * @file TaskDemoHandler.h
 * @brief 任务演示功能：双通道（自由任务 / 冻结任务）插件范式示例
 */
#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 任务演示：execute 按参数分流两条任务通道
 *
 * - 模式 0（自由任务 ctx.runJob）：10 段×200ms 心跳，不写模型、不遮罩，用户可继续只读交互与参数更新；
 * - 模式 1（冻结任务 ctx.runModelJob）：5 段×200ms 心跳后向目标组件影子加一点——
 *   忙碌遮罩盖下、提交进 undo（可撤销），红叉于下次心跳中止（影子丢弃、模型零变化）。
 * - 模式 2（回写任务 ctx.runTypedWriteback）：3 段×200ms 心跳后向目标组件加一点——
 *   不遮罩不冻结，回写段由框架发写面（undo 边界内解析目标，进 undo 可撤销）。
 *
 * 每次 report 即心跳（取消检查点）；无心跳的任务不可中断（诚实语义）。
 */
class TaskDemoHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;
};
}
