/**
 * @file TaskDemoHandler.cpp
 * @brief 任务演示功能实现（双通道插件范式）
 */
#include "TaskDemoHandler.h"
#include "ComponentData.h"
#include "ComponentOperator.h"
#include "FeatureContext.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "MeshData.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

namespace systems::feature {
namespace {
    constexpr const char* kFeatureName = "TaskDemo";
}

void TaskDemoHandler::setup(FeatureRegistrar& reg, FeatureContext&)
{
    reg.addParameter({ ArgTypeEnum::Int, "模式", "0",
        "0=自由任务（不写模型）；1=冻结任务（影子加点，可撤销）；2=自由任务+终态回写（进 undo）" });
    reg.addMenuItem({ "功能/演示", "任务演示" });
}

std::any TaskDemoHandler::execute(FeatureContext& ctx)
{
    const long long* mode = ctx.params.value(0).get<ArgTypeEnum::Int>();
    if (mode && *mode == 1) {
        // 冻结通道：目标组件经活动组件查询（演示简化；正式功能应经 Selector 参数解析）
        const std::optional<Index> target = ctx.activeComponent();
        if (!target) {
            spdlog::warn("TaskDemo: no active component for frozen demo (select a mesh component first)");
            return { };
        }
        auto job = ctx.runModelJob("冻结加点演示", *target,
            [](ComponentOperator& shadow_target, systems::job::ProgressFn report) {
                for (int i = 1; i <= 5; ++i) {
                    report(i / 5.0, "冻结阶段 " + std::to_string(i) + "/5");
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
                if (!shadow_target.component().mesh)
                    throw std::runtime_error("target has no mesh");
                const auto n = shadow_target.component().mesh->vertex_positions_.size();
                shadow_target.appendPoint({ static_cast<double>(n), 0.0, 0.0 });
                report(1.0, "已加点");
            });
        if (!job)
            spdlog::warn("TaskDemo: frozen job rejected (busy / geometry component / scope session / no runner)");
        return { };
    }

    if (mode && *mode == 2) {
        // 终态回写通道：自由任务（不遮罩不冻结）+ 绑定回写目标——写面由框架发
        // （提交段在 undo 边界内解析目标并守卫）；worker 不碰真实模型，undo/redo 与其他模型写在任务占用期拒绝
        const std::optional<Index> target = ctx.activeComponent();
        if (!target) {
            spdlog::warn("TaskDemo: no active component for writeback demo (select a mesh component first)");
            return { };
        }
        auto job = ctx.runTypedWriteback(
            "终态回写演示", *target,
            [](const ComponentOperator& op) {
                if (!op.mesh())
                    throw std::runtime_error("target has no mesh");
                return op.mesh()->vertex_positions_.size(); },
            [](std::size_t& point_count, systems::job::ProgressFn report) {
                for (int i = 1; i <= 3; ++i) {
                    report(i / 3.0, "计算阶段 " + std::to_string(i) + "/3");
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
                report(1.0, "计算完成，准备回写");
                return point_count; }, [](ComponentOperator& op, std::size_t& point_count, systems::job::ProgressFn report) {
                // 捕获已验证网格，任务占用期间目标不会被其他操作修改。
                op.appendPoint({ static_cast<double>(point_count), 1.0, 0.0 });
                report(1.0, "回写完成：已向目标组件添加一点。"); });
        if (!job)
            spdlog::warn("TaskDemo: writeback job rejected (slot busy / no runner)");
        return { };
    }

    // 自由通道：无写面、不遮罩——状态栏进度与红叉可用，用户可继续只读交互与参数更新
    auto job = ctx.runJob("自由任务演示",
        [](systems::job::ProgressFn report) {
            for (int i = 1; i <= 10; ++i) {
                report(i / 10.0, "自由阶段 " + std::to_string(i) + "/10");
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        });
    if (!job)
        spdlog::warn("TaskDemo: free job rejected (slot busy / no runner)");
    return { };
}
}
