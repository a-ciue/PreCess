#include "FeatureDemoHandler.h"
#include "ComponentOperator.h"
#include "FeatureContext.h"
#include "FeatureEvents.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "MeshData.h"

#include <spdlog/spdlog.h>

namespace systems::feature {
namespace {
    constexpr const char* kFeatureName = "FeatureDemo";
    // Qt 键码：model 层以 int 传递，此处对应 Qt::Key_D 与 Qt::ControlModifier
    constexpr int kKeyD = 'D';
    constexpr int kControlModifier = 0x04000000;
}

void FeatureDemoHandler::setup(FeatureRegistrar& reg, FeatureContext& ctx)
{
    // 功能参数注册
    reg.addParameter({ ArgTypeEnum::Float, "缩放因子", "1.0", "网格顶点坐标的缩放倍数" });
    reg.addParameter({ ArgTypeEnum::Bool, "自动应用", "false", "修改参数后立即对当前组件生效" });
    // 菜单选项注册：演示两级菜单路径，"功能" 菜单分页内的 "批处理" 分组
    reg.addMenuItem({ "功能/批处理", "功能示例" });
    // 演示页内分组竖线分隔："功能" 菜单分页内与 "批处理" 并列的 "测量" 分组
    // 同时演示自定义图标：指定 qrc 图标资源路径，未指定时按插件名映射默认图标
    reg.addMenuItem({ "功能/测量", "功能示例", "qrc:/images/toolbar/Algorithm/gmsh.svg" });
    // 演示单级菜单路径，归入 "示例" 菜单分页的默认分组
    reg.addMenuItem({ "示例", "功能示例" });
    // 按键事件注册：Ctrl+D
    reg.addKeyBinding({ kKeyD, kControlModifier });

    ctx_ = &ctx;

    // 订阅参数变更事件：功能对参数改变实时做出响应
    param_sub_ = ctx.events.subscribe<ParameterChangedEvent>([this](const ParameterChangedEvent& e) {
        if (e.feature != kFeatureName || !ctx_) {
            return;
        }
        syncParams(*ctx_);
        spdlog::info("FeatureDemo: param {} changed, scale={} auto_apply={}", e.param_index, scale_, auto_apply_);
        // 开启"自动应用"时，缩放因子修改立即作用到当前组件
        if (auto_apply_ && e.param_index == 0) {
            applyScale(*ctx_);
        }
    });

    // 订阅模型事件：功能对模型增删改实时做出响应
    model_sub_ = ctx.events.subscribe<ModelEvent>([](const ModelEvent& e) {
        spdlog::info("FeatureDemo: model event kind={} model={} component={}",
            static_cast<int>(e.kind), e.model_id, e.component_id);
    });

    spdlog::info("FeatureDemo: setup");
}

void FeatureDemoHandler::teardown(FeatureContext&)
{
    // 事件订阅句柄随成员析构自动退订，这里只需清理自身状态
    ctx_ = nullptr;
    spdlog::info("FeatureDemo: teardown");
}

void FeatureDemoHandler::activate(FeatureContext& ctx)
{
    // 进入功能时同步参数当前值：setup 返回后系统才载入参数默认值，故不在 setup 中同步
    syncParams(ctx);
}

std::any FeatureDemoHandler::execute(FeatureContext& ctx)
{
    applyScale(ctx);
    return {};
}

bool FeatureDemoHandler::onKeyEvent(const KeyEvent& event)
{
    spdlog::info("FeatureDemo: key binding triggered (key={})", static_cast<char>(event.key));
    if (ctx_) {
        applyScale(*ctx_);
    }
    return true; // 绑定命中即消费该事件
}

void FeatureDemoHandler::syncParams(FeatureContext& ctx)
{
    if (const auto* v = ctx.params.value(0).get<ArgTypeEnum::Float>()) {
        scale_ = *v;
    }
    if (const auto* v = ctx.params.value(1).get<ArgTypeEnum::Bool>()) {
        auto_apply_ = *v;
    }
}

void FeatureDemoHandler::applyScale(FeatureContext& ctx)
{
    // 经上下文动态获取当前活动组件，再申请组件操作句柄修改模型对象
    auto component_id = ctx.activeComponent ? ctx.activeComponent() : std::nullopt;
    if (!component_id) {
        spdlog::warn("FeatureDemo: no active component");
        return;
    }
    auto op = ctx.componentOperator ? ctx.componentOperator(*component_id) : std::nullopt;
    if (!op) {
        spdlog::warn("FeatureDemo: component {} not found", *component_id);
        return;
    }
    if (!op->mesh()) {
        spdlog::warn("FeatureDemo: component {} has no mesh", *component_id);
        return;
    }
    // 只改坐标不动拓扑：NonTopology 标脏（邻接懒表不失效）；
    // 顶点坐标常驻组件 MeshData 就地缩放，通知由操作边界 flush 统一发出
    MeshData& mesh = op->editableMesh(MeshEditKind::NonTopology);
    for (auto& point : mesh.vertex_positions_) {
        point[0] *= scale_;
        point[1] *= scale_;
        point[2] *= scale_;
    }
    spdlog::info("FeatureDemo: scaled component {} by {}", *component_id, scale_);
}
}
