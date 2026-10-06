/**
 * @file FeatureContext.h
 * @brief 功能上下文：功能访问软件环境的入口
 */
#ifndef FEATURE_CONTEXT_H
#define FEATURE_CONTEXT_H
#include "ComponentOperator.h" // std::optional<ComponentOperator> 需要完整类型
#include "Core.h"
#include "FeatureEventGateway.h" // events 成员类型（功能经其订阅事件，调用点需完整类型）
#include "Job.h" // runComputeJob 成员类型（systems::job::Job / JobTaskFn）

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

class ModelLayer;

namespace systems::feature {
class FeatureParams;
class InteractionContext;
class FeatureSystem;
struct FeatureContext;

//! 影子组件任务体：写面 = 影子目标组件的操作句柄（scope 限目标组件，无 io、无其他组件访问）
//! report 为心跳（含取消检查）；写入落影子层，由框架在 undo 边界内提交/丢弃
using ComponentJobTaskFn = std::function<void(ComponentOperator& shadow_target, systems::job::ProgressFn report)>;

//! GUI 回写段：框架在发布前校验目标，在持有操作占用的提交段交付就绪写面。
using WritebackFn = std::function<void(ComponentOperator&, systems::job::ProgressFn)>;
//! GUI 准备段：框架持有模型操作占用，按目标只读捕获输入，返回纯计算任务。
using CaptureJobFn = std::function<systems::job::JobTaskFn(const ComponentOperator&)>;
//! GUI 准备与提交覆盖整个模型层；worker 仍只拿自持输入和结果。
using LayerCaptureJobFn = std::function<systems::job::JobTaskFn(const ModelLayer&)>;
using LayerWritebackFn = std::function<void(ModelLayer&, systems::job::ProgressFn)>;

/**
 * @brief 临时预览视图：插件控制两次 execute 之间的开层、重试和取消。
 *
 * 捕获由 UndoStack 持有。事件回调只更新自己的层，不维护正式历史；
 * execute 收尾由框架吸收层的最早前像与最终效果，关闭层并形成操作记录。
 * 插件无需提交层或配对正式操作边界。
 */
class UndoContext {
public:
    //! @brief 开启本功能预览，捕获覆盖层内组件与结构修改；无栈返回 false。
    bool beginScope(std::string label);
    //! @brief 回滚关闭本功能预览；忙时请求取消并由框架延后清理。
    void cancelScope();
    //! @brief 回滚本功能预览，保持层身份供重试。
    void revertScope();
    //! @brief 本功能是否有进行中的预览。
    bool scopeActive() const;

private:
    friend struct FeatureContext;
    UndoContext(FeatureSystem& system, std::string owner);
    FeatureSystem& system_;
    const std::string owner_;
};

/**
 * @brief 功能上下文，由 FeatureSystem 装配并在 setup()/activate()/execute() 时传给功能
 *
 * 固定绑定所属系统与功能身份；框架服务始终可调用，查询结果可为空。
 * 宿主动态 provider 由系统持有，晚装配仍生效。功能层不反向依赖 app 层。
 *
 * @note activeModel/activeComponent 是对象树选中态的动态查询，只视作一种提示：
 *       不要强制要求用户执行功能前在对象树中选中 component；目标组件优先经
 *       `Selector` 类型参数（FeatureParams）让用户显式选择后解析。
 */
struct FeatureContext {
    ModelLayer& model; //> 模型层入口
    FeatureEventGateway& events; //> 事件网关，功能在 setup() 中订阅事件（回调返回后自动 flush 组件变更通知）
    FeatureParams& params; //> 本功能的持久参数集
    InteractionContext& interaction; //> 视口交互入口，功能在 setup() 中订阅拾取/悬停回调
    //! @brief 查询对象树当前模型提示；无 provider 返回空。
    std::optional<Index> activeModel() const;
    //! @brief 查询对象树当前组件提示；无 provider 返回空。
    std::optional<Index> activeComponent() const;
    //! @brief 按身份查询所属模型的组件操作句柄。
    std::optional<ComponentOperator> componentOperator(Index component_id) const;
    UndoContext undo; //> undo 上下文（插件层接口入口；语义见 UndoContext 注释）
    /**
     * @brief 纯计算任务；不得持活模型句柄，无模型回写与 undo 记录。
     * 输入须在 GUI 复制，失败抛异常。任务到 GUI 清理结束持续占用模型；忙或未装配时返回空。
     * 进度是计算线程的取消检查点；提交、清理与终态均由框架回所属线程完成。
     */
    std::shared_ptr<systems::job::Job> runComputeJob(std::string label, systems::job::JobTaskFn task);
    /**
     * @brief 单组件影子修改；仅正式入口且无预览时发布，GUI 自动提交或丢弃。
     * worker 只操作影子写面，失败抛异常；几何／映射目标暂不支持。遮罩只影响展示。
     */
    std::shared_ptr<systems::job::Job> runComponentJob(std::string label, Index component_id, ComponentJobTaskFn task);

    /** @brief 由系统构造完整上下文，所属模型与功能身份不再通过闭包装配。 */
    FeatureContext(FeatureSystem& system, std::string owner, FeatureEventGateway& events,
        FeatureParams& params, InteractionContext& interaction);

    /** @brief 输入已准备的类型化回写；masked 仅控制交互遮罩，默认不遮罩。 */
    template <typename Compute, typename Write>
        requires std::is_invocable_v<Compute, systems::job::ProgressFn>
        && !std::is_void_v<std::invoke_result_t<Compute, systems::job::ProgressFn>>
        && std::is_invocable_v<Write, ComponentOperator&,
            std::invoke_result_t<Compute, systems::job::ProgressFn>&, systems::job::ProgressFn>
    std::shared_ptr<systems::job::Job> runWritebackJob(
        std::string label, Index component_id, Compute compute, Write write, bool masked = false)
    {
        return runWritebackJob(std::move(label), component_id, [](const ComponentOperator&) { return 0; }, [compute = std::move(compute)](int&, systems::job::ProgressFn report) mutable { return compute(std::move(report)); }, std::move(write), masked);
    }

    /**
     * @brief GUI 捕获输入 → worker 计算 → GUI 回写。插件只提供业务函数，不传递通用结果包。
     * 框架先占用并交付只读目标；compute 抛异常或取消则不调用 write。
     * 正式／预览归属在发布时固定；预览回写只进入原身份的层。
     * compute/write 都接收 report；计算上报是取消心跳，提交上报只更新展示。
     * masked 仅控制交互遮罩，不改变模型占用、取消与记账规则。
     */
    template <typename Capture, typename Compute, typename Write>
        requires std::is_invocable_v<Capture, const ComponentOperator&>
    std::shared_ptr<systems::job::Job> runWritebackJob(
        std::string label, Index component_id, Capture capture, Compute compute, Write write, bool masked = false)
    {
        using Input = std::decay_t<std::invoke_result_t<Capture, const ComponentOperator&>>;
        using Result = std::decay_t<std::invoke_result_t<Compute, Input&, systems::job::ProgressFn>>;
        // 访问阶段由 Runner 定序；一个载体同时持有输入与结果，不再分别共享。
        struct Payload {
            std::optional<Input> input;
            std::optional<Result> result;
        };
        auto payload = std::make_shared<Payload>();
        return runCapturedWriteback(std::move(label), component_id, [capture = std::move(capture), compute = std::move(compute), payload](const ComponentOperator& op) {
                payload->input.emplace(capture(op));
                return systems::job::JobTaskFn {
                    [compute, payload](systems::job::ProgressFn report) mutable {
                        payload->result.emplace(compute(*payload->input, std::move(report)));
                    }
                }; }, [write = std::move(write), payload](ComponentOperator& op, systems::job::ProgressFn report) mutable { write(op, *payload->result, std::move(report)); }, masked);
    }

    /**
     * @brief 通用模型层回写：GUI 捕获多目标输入，worker 计算，GUI 安装结果。
     * 不要求已有目标或网格；模型增删、几何结果及多组件写共用原操作归属。
     * 几何内部处理和输入生命周期由插件维护，框架不自动复制底层形状。
     */
    template <typename Capture, typename Compute, typename Write>
        requires std::is_invocable_v<Capture, const ModelLayer&>
    std::shared_ptr<systems::job::Job> runWritebackJob(
        std::string label, Capture capture, Compute compute, Write write, bool masked = false)
    {
        using Input = std::decay_t<std::invoke_result_t<Capture, const ModelLayer&>>;
        using Result = std::decay_t<std::invoke_result_t<Compute, Input&, systems::job::ProgressFn>>;
        struct Payload {
            std::optional<Input> input;
            std::optional<Result> result;
        };
        auto payload = std::make_shared<Payload>();
        return runCapturedWriteback(std::move(label), [capture = std::move(capture), compute = std::move(compute), payload](const ModelLayer& layer) {
                payload->input.emplace(capture(layer));
                return systems::job::JobTaskFn {
                    [compute, payload](systems::job::ProgressFn report) mutable {
                        payload->result.emplace(compute(*payload->input, std::move(report)));
                    }
                }; }, [write = std::move(write), payload](ModelLayer& layer, systems::job::ProgressFn report) mutable { write(layer, *payload->result, std::move(report)); }, masked);
    }

private:
    //! @brief 类型化入口复用系统的目标捕获、占用与提交规则。
    std::shared_ptr<systems::job::Job> runCapturedWriteback(std::string label, Index component_id,
        CaptureJobFn capture, WritebackFn write, bool masked);
    std::shared_ptr<systems::job::Job> runCapturedWriteback(std::string label,
        LayerCaptureJobFn capture, LayerWritebackFn write, bool masked);
    FeatureSystem& system_;
    const std::string owner_;
};
}
#endif // FEATURE_CONTEXT_H
