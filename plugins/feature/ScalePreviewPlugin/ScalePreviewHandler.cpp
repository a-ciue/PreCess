#include "ScalePreviewHandler.h"
#include "ComponentOperator.h"
#include "FeatureContext.h"
#include "FeatureEvents.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "MeshData.h"

#include <spdlog/spdlog.h>

#include <cstddef>

namespace systems::feature {
namespace {
    constexpr const char* kFeatureName = "ScalePreview";
    // 参数下标：0 缩放因子（Float），1 预览（Button），2 取消（Button）
    constexpr std::size_t kParamScale = 0;
    constexpr std::size_t kParamPreview = 1;
    constexpr std::size_t kParamCancel = 2;
}

void ScalePreviewHandler::setup(FeatureRegistrar& reg, FeatureContext& ctx)
{
    // 功能参数注册
    reg.addParameter({ ArgTypeEnum::Float, "缩放因子", "1.0", "网格顶点坐标的缩放倍数（预览按绝对因子应用，不累计）" });
    // Button 为无值触发器：计数器载荷，功能约定忽略值、只读参数下标
    reg.addParameter({ ArgTypeEnum::Button, "预览", "", "开启预览：beginStaged 捕获 before₀ 并开始监听参数变化" });
    reg.addParameter({ ArgTypeEnum::Button, "取消", "", "取消预览：回滚到 before₀，不成记录" });
    // 菜单选项注册：归入 "示例" 菜单分页的默认分组（菜单触发 = 确认预览成一条 undo 记录）
    reg.addMenuItem({ "示例", "缩放预览演示" });

    ctx_ = &ctx;

    // 订阅参数变更事件：预览按钮开启会话（开始监听），因子变更重试预览，取消按钮收尾
    param_sub_ = ctx.events.subscribe<ParameterChangedEvent>([this](const ParameterChangedEvent& e) {
        if (e.feature != kFeatureName || !ctx_) {
            return;
        }
        FeatureContext& ctx = *ctx_;
        if (e.param_index == kParamScale) {
            syncParams(ctx);
            // staged 打开时重试预览：回滚到 before₀ 再按新因子重写（绝对因子，不累计）；
            // 未打开时只更新参数值，不改模型
            if (ctx.undo.stagedActive()) {
                ctx.undo.revertStaged();
                applyPreview(ctx);
            } else {
                spdlog::info("ScalePreview: scale factor changed to {} without staged session, model untouched", scale_);
            }
        } else if (e.param_index == kParamPreview) {
            // 预览：开 staged 会话开始监听参数变化（已有会话 = 按当前因子重预览）
            spdlog::info("ScalePreview: start preview (staged={})", ctx.undo.stagedActive());
            startPreview(ctx);
        } else if (e.param_index == kParamCancel) {
            // 取消：恢复 before₀ 并关闭会话，不成记录（无会话时空转容忍）
            spdlog::info("ScalePreview: cancel (staged={})", ctx.undo.stagedActive());
            ctx.undo.cancelStaged();
        }
    });

    spdlog::info("ScalePreview: setup");
}

void ScalePreviewHandler::teardown(FeatureContext& ctx)
{
    // 注销兜底：退出路径（deactivate）已关闭 staged 会话，此处仅容错空转
    if (ctx.undo.stagedActive()) {
        ctx.undo.cancelStaged();
    }
    // 事件订阅句柄随成员析构自动退订，这里只需清理自身状态
    ctx_ = nullptr;
    spdlog::info("ScalePreview: teardown");
}

void ScalePreviewHandler::deactivate(FeatureContext& ctx)
{
    // 插件职责：功能退出时关闭 staged 会话（AGENTS.md 约定；真实写入点另有隐式 cancel 兜底）
    if (ctx.undo.stagedActive()) {
        ctx.undo.cancelStaged();
    }
}

std::any ScalePreviewHandler::execute(FeatureContext& ctx)
{
    // 菜单触发 = 确认预览：before₀ + 当前状态成一条 undo 记录（无会话时空转容忍）
    spdlog::info("ScalePreview: confirm via execute (staged={})", ctx.undo.stagedActive());
    ctx.undo.commitStaged();
    return {};
}

void ScalePreviewHandler::startPreview(FeatureContext& ctx)
{
    // 组件与网格检查在 beginStaged 之前：预览无从应用时不开会话
    auto component_id = ctx.activeComponent ? ctx.activeComponent() : std::nullopt;
    if (!component_id) {
        spdlog::warn("ScalePreview: no active component");
        return;
    }
    auto op = ctx.componentOperator ? ctx.componentOperator(*component_id) : std::nullopt;
    if (!op || !op->mesh()) {
        spdlog::warn("ScalePreview: component {} not found or has no mesh", *component_id);
        return;
    }
    // 已有会话先回滚到 before₀（重按预览 = 按当前因子重预览）；
    // 无 UndoStack 注入时 beginStaged 返回 false，降级为警告并跳过预览
    if (ctx.undo.stagedActive()) {
        ctx.undo.revertStaged();
    } else if (!ctx.undo.beginStaged("缩放预览", *component_id)) {
        spdlog::warn("ScalePreview: beginStaged failed (component missing or no undo stack), preview skipped");
        return;
    }
    applyPreview(ctx);
}

void ScalePreviewHandler::syncParams(FeatureContext& ctx)
{
    if (const auto* v = ctx.params.value(kParamScale).get<ArgTypeEnum::Float>()) {
        scale_ = *v;
    }
}

bool ScalePreviewHandler::applyPreview(FeatureContext& ctx)
{
    // 经上下文动态获取当前活动组件，再申请组件操作句柄修改模型对象
    auto component_id = ctx.activeComponent ? ctx.activeComponent() : std::nullopt;
    if (!component_id) {
        spdlog::warn("ScalePreview: no active component");
        return false;
    }
    auto op = ctx.componentOperator ? ctx.componentOperator(*component_id) : std::nullopt;
    if (!op) {
        spdlog::warn("ScalePreview: component {} not found", *component_id);
        return false;
    }
    if (!op->mesh()) {
        spdlog::warn("ScalePreview: component {} has no mesh", *component_id);
        return false;
    }
    // 只改坐标不动拓扑：NonTopology 标脏（邻接懒表不失效）；revertStaged 保证总是
    // 从 before₀ 起按绝对因子缩放，不累计。预览写的通知由 invoke 边界/事件网关 flush
    // 统一发出（Manual 模式只 flush 不成记录）
    MeshData& mesh = op->editableMesh(MeshEditKind::Topology);
    for (auto& point : mesh.vertex_positions_) {
        point[0] *= scale_;
        point[1] *= scale_;
        point[2] *= scale_;
    }
    spdlog::info("ScalePreview: preview component {} scaled by {}", *component_id, scale_);
    return true;
}
}
