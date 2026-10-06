/**
 * @file TaskDemoHandler.h
 * @brief 任务演示功能：计算任务、影子组件任务与回写任务范式示例
 */
#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 任务演示：execute 按参数选择三类任务
 *
 * - 模式 0（纯计算任务 ctx.runComputeJob）：10 段×200ms 心跳，不写模型、不遮罩，用户可继续只读交互与参数更新；
 * - 模式 1（影子组件任务 ctx.runComponentJob）：5 段×200ms 心跳后向目标组件影子加一点——
 *   忙碌遮罩盖下、提交进 undo（可撤销），红叉于下次心跳中止（影子丢弃、模型零变化）。
 * - 模式 2（回写任务 ctx.runWritebackJob）：3 段×200ms 心跳后向目标组件加一点——
 *   不遮罩，仍占用模型写权；GUI 回写由框架提供写面并纳入 undo。
 *
 * 每次 report 即心跳（取消检查点）；无心跳的任务不可中断（诚实语义）。
 */
class TaskDemoHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;
};
}
