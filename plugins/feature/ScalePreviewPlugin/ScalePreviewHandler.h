#pragma once
#include "ComponentOperator.h" // 写面缓存成员（std::optional 成员需完整类型）
#include "Core.h"
#include "EventBus.h"
#include "FeatureHandler.h"

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace systems::job {
class Job;
}

namespace systems::feature {
class FeatureContext;

/**
 * @brief 缩放预览演示功能：插件层预览机制 + 工作线程非阻塞预览
 *
 * 预览按钮开临时层，参数事件与后台回写更新预览，不产生正式记录。
 * execute 使用最终预览，框架收尾吸收捕获并关闭层；取消按钮回滚关闭。
 * 输入坐标在 GUI 拷贝，worker 分块计算，GUI 回写校验发布时的预览身份。
 * 绝对因子基于预览起点，不累计缩放；重按预览回滚后重建基准。
 *
 * 失败/忙碌路径：执行器未注入或单槽被占 → 按框架契约直接丢弃（不计算、不写模型）——
 * 异步 Session 装配执行器，单槽忙是正常背压；
 * 不得在 GUI 线程补算（绕过任务系统管辖，且占槽任务必开软冻结窗、补算写入必被写闸拒绝）；
 * 每轮任务固定因子，GUI 只落计算结果；终态后模型通知回放按最新参数接续下一轮。
 * 未开预览直接 execute 也走后台计算，并启用交互遮罩；成功提交才形成正式记录。
 * 确认（execute）在预览任务在飞时拒绝，待终态后重试。
 */
class ScalePreviewHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    void teardown(FeatureContext& ctx) override;
    void deactivate(FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;

private:
    using PreviewPositions = std::vector<std::array<double, 3>>; //!< 缩放数据形态

    //! 预览共享状态：两次 execute 之间存活（任务闭包经 shared_ptr 持有；任务作废经 cancel，退出由框架取消）
    struct PreviewState {
        PreviewPositions base; //!< 会话起点 before₀ 坐标（发布后只读）
        std::optional<double> applied_factor; //!< 已落地的因子，只在 GUI 访问
    };

    //! 单轮预览计算结果（任务体产出，经 runTypedWriteback 由框架直传回写段——免共享状态发布）
    struct PreviewResult {
        PreviewPositions out; //!< 分块缩放结果（本轮最终值）
        double computed { 1.0 }; //!< 本轮计算所用因子
    };

    /**
     * @brief 缩放核心（全库唯一缩放循环）：base 每点乘因子 f 写入 out——尺寸随 base
     *
     * 仅由 worker 调用；每满一块回调进度比、收尾回调 1.0，均为取消检查点。
     */
    static void scaleInto(const PreviewPositions& base, double f, PreviewPositions& out,
        std::function<void(double)> on_chunk);

    //! 预览任务是否在飞（已提交未到终态）
    bool previewInFlight() const;

    // 状态同步统一入口：从参数集同步最新缩放因子，
    // 并解析活动组件写面缓存（execute/开预览直接用——解析不散落在各调用点）
    void syncParams(FeatureContext& ctx);
    // 开层并按当前因子提交预览任务（已有层先回滚 before₀ 再重启）
    void startPreview(FeatureContext& ctx);
    // 提交单轮预览；本功能任务在飞时仅保留最新参数，终态模型通知再接续。
    void submitPreviewJob(FeatureContext& ctx);
    // 终态回写（GUI 线程）：框架保证预览归属，插件复核基准尺寸并移动结果。
    // op = 框架解析好的目标写面（组件存在、网格在场由框架保证）
    void writePreview(PreviewState& state, PreviewResult& result, ComponentOperator& op);
    // 作废在飞预览任务（cancel 令牌，通道拦截排队回写；功能退出另有框架取消兜底）
    void abandonPreviewJob();

    FeatureContext* ctx_ { nullptr }; //> 功能上下文（生命周期由 FeatureSystem 管理，先于此 handler 销毁）
    core::EventBus::Subscription param_sub_; //> 参数变更事件订阅
    core::EventBus::Subscription model_sub_; //!< 终态模型通知接续预览计算
    double scale_ { 1.0 }; //> "缩放因子"参数的当前值
    Index active_target_ { -1 }; //> 活动组件 id（syncParams 解析；仅同步后的同一事件内使用）
    std::optional<ComponentOperator> active_op_; //!< 活动组件写面缓存（syncParams 每次刷新——组件删除发生在事件之间，下次同步必先刷新，不会持旧悬垂值）
    Index preview_component_ { -1 }; //> 预览目标组件 id（会话期有效）
    std::shared_ptr<PreviewState> preview_; //> 当前会话共享状态
    std::shared_ptr<systems::job::Job> preview_job_; //> 在飞预览任务（终态判断/作废用）
};
}
