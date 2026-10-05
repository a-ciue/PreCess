#include "ScalePreviewHandler.h"
#include "ComponentOperator.h"
#include "FeatureContext.h"
#include "FeatureEvents.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "Job.h"
#include "MeshData.h"

#include <spdlog/spdlog.h>

#include <cstddef>
#include <utility>

namespace systems::feature {
namespace {
    constexpr const char* kPreviewLabel = "缩放预览";
    // 参数下标：0 缩放因子（Float），1 预览（Button），2 取消（Button）
    constexpr std::size_t kParamScale = 0;
    constexpr std::size_t kParamPreview = 1;
    constexpr std::size_t kParamCancel = 2;
    // 分块粒度：每满一块一次心跳（进度/取消检查点，任务可被红叉中断）
    constexpr std::size_t kChunkPoints = 1u << 20;
    constexpr const char* kComputingLabel = "缩放预览计算中";
}

void ScalePreviewHandler::setup(FeatureRegistrar& reg, FeatureContext& ctx)
{
    // 功能参数注册
    reg.addParameter({ ArgTypeEnum::Float, "缩放因子", "1.0", "网格顶点坐标的缩放倍数（预览按绝对因子应用，不累计）" });
    // Button 为无值触发器：计数器载荷，功能约定忽略值、只读参数下标
    reg.addParameter({ ArgTypeEnum::Button, "预览", "", "开启预览：beginScope 开层（层内改动由栈捕获）并开始监听参数变化" });
    reg.addParameter({ ArgTypeEnum::Button, "取消", "", "取消预览：按层内捕获回滚，不成记录（在飞计算一并作废）" });
    // 菜单选项注册：归入 "示例" 菜单分页的默认分组
    // （菜单触发 = 会话态确认预览成记录；无会话直接按因子缩放并成一条记录）
    reg.addMenuItem({ "示例", "缩放预览演示" });

    ctx_ = &ctx;

    // 订阅参数变更事件：预览按钮开启会话（开始监听），因子变更重算预览，取消按钮收尾
    param_sub_ = ctx.events.subscribe<ParameterChangedEvent>([this](const ParameterChangedEvent& e) {
        if (!ctx_) {
            return;
        }
        FeatureContext& ctx = *ctx_;
        if (e.param_index == kParamScale) {
            syncParams(ctx);
            // 层打开时提交预览任务（工作线程重算 + 终态回写；绝对因子按开层时状态计算，
            // 覆盖式写入——无需每轮 revertScope）；未打开时只更新参数值，不改模型
            if (ctx.undo.scopeActive()) {
                submitPreviewJob(ctx);
            } else {
                spdlog::info("ScalePreview: scale factor changed to {} without scope session, model untouched", scale_);
            }
        } else if (e.param_index == kParamPreview) {
            // 预览：开层开始监听参数变化（已有层 = 按当前因子重预览）
            spdlog::info("ScalePreview: start preview (scope={})", ctx.undo.scopeActive());
            startPreview(ctx);
        } else if (e.param_index == kParamCancel) {
            // 取消：作废在飞计算（排队回写被任务通道拦截），按层内捕获回滚并关闭会话，不成记录
            spdlog::info("ScalePreview: cancel (scope={})", ctx.undo.scopeActive());
            abandonPreviewJob();
            ctx.undo.cancelScope();
        }
    });

    spdlog::info("ScalePreview: setup");
}

void ScalePreviewHandler::teardown(FeatureContext&)
{
    // 作废在飞预览任务（teardown 后回写闭包不得再触模型）；层关闭由框架兜底
    // （功能退出路径在插件回调后统一 cancelScope——业务性开/确认/重按仍由本插件自控）
    abandonPreviewJob();
    ctx_ = nullptr;
    spdlog::info("ScalePreview: teardown");
}

void ScalePreviewHandler::deactivate(FeatureContext&)
{
    // 任务取消（功能退出框架统一 cancelPendingJobs）与层关闭（插件回调后兜底）
    // 均已接管；此处仅作废本功能预览任务引用（幂等——取消按钮/重按的主动放弃仍在）
    abandonPreviewJob();
}

std::any ScalePreviewHandler::execute(FeatureContext& ctx)
{
    // 框架在任务占用期拒绝 execute，确认到达此处时回写与模型清理均已完成。
    // 会话态 = 确认预览：开层前状态 + 当前状态成一条 undo 记录
    if (ctx.undo.scopeActive()) {
        spdlog::info("ScalePreview: confirm via execute (scope=true)");
        preview_.reset();
        return { };
    }
    // 无会话（未点预览直接执行）：直接按当前因子缩放并记一条 undo 记录——
    // 语义为对当前坐标的增量缩放（无会话即无 before₀ 绝对因子基准）；非阻塞预览仍走"预览"按钮。
    syncParams(ctx); // 参数与目标写面一并刷新
    spdlog::info("ScalePreview: no scope session, applying scale {} directly", scale_);
    if (!active_op_) {
        spdlog::warn("ScalePreview: no active component or mesh (direct scale)");
        return { };
    }
    // 开层后同步缩放；框架在 execute 收尾确认，不需插件提交。
    if (!ctx.undo.beginScope(kPreviewLabel)) {
        spdlog::warn("ScalePreview: beginScope refused (no undo stack or restore in progress), direct scale skipped");
        return { };
    }
    MeshData& mesh = active_op_->editableMesh(MeshEditKind::NonTopology);
    scaleInto(mesh.vertex_positions_, scale_, mesh.vertex_positions_); // 就地缩放（同址）
    // 层由框架在 execute 收尾并入记录并关闭。
    return { };
}

void ScalePreviewHandler::startPreview(FeatureContext& ctx)
{
    syncParams(ctx); // 预览按钮可能晚于因子设置：参数与目标写面一并刷新
    // 组件与网格检查在 beginScope 之前：预览无从应用时不开会话（写面经 syncParams 解析）
    if (!active_op_) {
        spdlog::warn("ScalePreview: no active component or mesh (preview start)");
        return;
    }
    // 作废上一会话的在飞任务（取消令牌 → 通道拦下排队回写）
    abandonPreviewJob();
    // 已有会话先回滚到开层时状态（重按预览 = 按当前因子重启会话）；
    // 无 UndoStack 注入时 beginScope 返回 false，降级为警告并跳过预览
    if (ctx.undo.scopeActive()) {
        ctx.undo.revertScope();
    } else if (!ctx.undo.beginScope(kPreviewLabel)) {
        spdlog::warn("ScalePreview: beginScope refused (no undo stack or restore in progress), preview skipped");
        return;
    }

    // 会话起点坐标值拷贝：任务闭包经 shared_ptr 共享只读（"读须发布前拷贝"契约）；
    // 拷贝后每轮因子变更不再需要 revertScope（重算自 before₀、覆盖式回写）
    auto state = std::make_shared<PreviewState>();
    state->base = active_op_->mesh()->vertex_positions_;
    state->factor.store(scale_);
    preview_ = std::move(state);
    preview_component_ = active_target_;

    submitPreviewJob(ctx);
}

void ScalePreviewHandler::submitPreviewJob(FeatureContext& ctx)
{
    if (!preview_)
        return;
    // 本功能任务在飞：置 pending——任务体一轮算完后按最新因子循环重算（自然背压）
    if (previewInFlight()) {
        preview_->pending.store(true);
        return;
    }

    auto state = preview_;
    auto job = ctx.runTypedWriteback(kPreviewLabel, preview_component_,
        // 任务体在 worker 只读 state（"读须发布前拷贝"契约），不解引用 handler；
        // 结果经框架槽自任务体直传回写段（免共享状态发布）
        [state](systems::job::ProgressFn report) {
            double computed = 0.0;
            PreviewPositions out;
            // 分块缩放（缩放核心经 on_chunk 每块心跳 + 收尾 1.0）；在飞期间来了新因子
            // 则循环重算，直到一轮内无新变更（自然背压）。静态调用不捕获 handler
            do {
                computed = state->factor.load();
                scaleInto(state->base, computed, out,
                    [&report](double ratio) { report(ratio, kComputingLabel); });
            } while (state->pending.exchange(false));
            return PreviewResult { std::move(out), computed }; }, [this, state](ComponentOperator& op, PreviewResult& result) {
            // 框架保证 handler、目标与发布时的预览身份，取消后不触达回写。
            writePreview(*state, result, op); });
    if (job) {
        preview_job_ = std::move(job);
        spdlog::debug("ScalePreview: preview job submitted");
    } else {
        // 框架契约：单槽忙或执行器未注入 → 直接丢弃（FeatureContext::runJob 契约原文
        // "发布方按设计直接丢弃"）。不在 GUI 线程补算：那绕过任务系统的进度/取消/提交门
        // 管辖，且占槽任务必开软冻结窗——补算的写入必被写闸拒绝（异常反噬事件回调链）。
        // 会话与 base 保留：槽空后的下一笔因子变更自然接上正常路径
        spdlog::warn("ScalePreview: preview discarded (job channel unavailable: runner missing or slot busy)");
    }
}

void ScalePreviewHandler::writePreview(const PreviewState& state, PreviewResult& result, ComponentOperator& op)
{
    // 目标解析已收归框架（runTypedWriteback 提交段保证组件存在、网格在场）
    // 尺寸守卫先于标脏（写前标脏契约：幂等提前返回路径不得标脏）：
    // 任务期模型互斥由框架保证；仍核对跨多轮预览的业务基准尺寸
    const auto& base = state.base;
    if (op.mesh()->vertex_positions_.size() != base.size()) {
        spdlog::warn("ScalePreview: preview write skipped (mesh changed during compute)");
        return;
    }

    // 收尾复核最新因子：提交窗口期到达的最后一笔变更按最新值写（兜底竞态）
    const double f_eff = state.factor.load();
    MeshData& mesh = op.editableMesh(MeshEditKind::NonTopology);
    if (f_eff == result.computed) {
        mesh.vertex_positions_ = std::move(result.out); // 类型化成功结果直接移动落地。
    } else {
        // 工作线程结果已过期（罕见收尾竞态）：按最新因子就地重算写入
        scaleInto(base, f_eff, mesh.vertex_positions_);
    }
    spdlog::info("ScalePreview: preview component {} scaled by {}", preview_component_, f_eff);
}

void ScalePreviewHandler::abandonPreviewJob()
{
    if (previewInFlight())
        preview_job_->cancel(); // 心跳/返回后/排队期拦截生效；排队中的回写由任务通道令牌拦下
    preview_job_.reset();
}

bool ScalePreviewHandler::previewInFlight() const
{
    return preview_job_ && !systems::job::isTerminal(preview_job_->state());
}

void ScalePreviewHandler::syncParams(FeatureContext& ctx)
{
    if (const auto* v = ctx.params.value(kParamScale).get<ArgTypeEnum::Float>()) {
        scale_ = *v;
    }
    if (preview_)
        preview_->factor.store(scale_); // 在飞任务与回写段读共享因子

    // 活动组件写面解析（与参数同为"从上下文刷新本类状态"）：execute/开预览直接用本缓存。
    // 缓存只在同步调用后的同一事件内使用——GUI 单线程，组件删除发生在事件之间，
    // 下次 syncParams 必先刷新，不会拿到悬垂写面；解析失败置空，由调用点带上下文报错
    active_op_.reset();
    active_target_ = -1;
    const auto id = ctx.activeComponent();
    if (!id)
        return;
    const auto op = ctx.componentOperator(*id);
    if (!op || !op->mesh())
        return;
    active_target_ = *id;
    active_op_ = op;
}

void ScalePreviewHandler::scaleInto(const PreviewPositions& base, double f, PreviewPositions& out,
    std::function<void(double)> on_chunk)
{
    const std::size_t n = base.size();
    out.resize(n); // 同址调用（就地缩放）时尺寸相同：无重分配，base 引用保持有效
    for (std::size_t i = 0; i < n; ++i) {
        out[i] = { base[i][0] * f, base[i][1] * f, base[i][2] * f };
        if (on_chunk && (i + 1) % kChunkPoints == 0)
            on_chunk(static_cast<double>(i + 1) / static_cast<double>(n));
    }
    if (on_chunk)
        on_chunk(1.0); // 收尾进度（任务体每轮心跳收口）
}
}
