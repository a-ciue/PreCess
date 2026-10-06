#pragma once
#include "AlgorithmHandler.h"

namespace systems::algo {
/**
 * @brief 进度/取消演示算法：10 个阶段各阻塞 200ms 并上报进度
 *
 * 每次上报即心跳（JobRunner 的包裹回调在此检查取消令牌）：
 * 执行期间点状态栏"取消"红叉，最迟约 200ms 内于下一心跳中止，
 * 状态栏随之显示"已取消"。不写模型——影子提交为空操作。
 *
 * @sa QAlgorithmSystemAdaptor::cancel
 */
class ProgressDemoHandler : public AlgorithmHandler {
public:
    std::any execute(HandlerContext& context, const std::vector<core::ArgObject>& args) override;
    std::vector<core::ArgType> args_type() const override;
};
}
