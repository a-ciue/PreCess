#include "ProgressDemoHandler.h"

#include <chrono>
#include <thread>

namespace systems::algo {
std::any ProgressDemoHandler::execute(HandlerContext& context, const std::vector<core::ArgObject>& /*args*/)
{
    constexpr int kStages = 10;
    constexpr auto kStageInterval = std::chrono::milliseconds(200);

    for (int i = 0; i < kStages; ++i) {
        // 上报即心跳：取消令牌在此检查，中止时抛 JobCancelledException 沿调用链传播
        context.report_progress(static_cast<double>(i) / kStages,
            "阶段 " + std::to_string(i + 1) + "/" + std::to_string(kStages));
        std::this_thread::sleep_for(kStageInterval);
    }
    context.report_progress(1.0, "完成");
    return {};
}

std::vector<core::ArgType> ProgressDemoHandler::args_type() const
{
    return {};
}
}
